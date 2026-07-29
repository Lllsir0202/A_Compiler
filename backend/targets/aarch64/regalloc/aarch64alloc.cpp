#include "backend/targets/aarch64/regalloc/aarch64alloc.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include "backend/targets/aarch64/regs/aarch64reg.h"
#include "backend/targets/utils/operand/targetreg.h"
#include "backend/targets/utils/struct/targetblock.h"
#include "backend/targets/utils/struct/targetfunction.h"

namespace backend::targetir::aarch64 {

namespace {
constexpr auto SP = static_cast<uint8_t>(GPR::Sp);
constexpr auto X16 = static_cast<uint8_t>(GPR::X16);  // address scratch (reserved)
constexpr int FRAME_OFFSET_MAX = 4095;                // safe base+offset range for 8-aligned slots

// Undirected edge key for the interference membership set.
auto EdgeKey(int u, int v) -> long long {
  if (u > v) {
    std::swap(u, v);
  }
  return (static_cast<long long>(u) << 32) | static_cast<unsigned>(v);
}
}  // namespace

auto AArch64GraphColorRegAlloc::Run(TargetFunction *func, size_t spill_base) -> size_t {
  func_ = func;
  spill_base_ = spill_base;
  spill_count_ = 0;
  spill_slot_.clear();
  spill_temps_.clear();
  assigned_.clear();
  used_callee_saved_.clear();

  AllocateClass(TargetRegClass::General);
  AllocateClass(TargetRegClass::Float);
  ApplyColors();
  return spill_count_;
}

auto AArch64GraphColorRegAlloc::IsInClass(TargetOperand *op) const -> bool {
  if (op->IsVirtualReg()) {
    return static_cast<TargetVirtualReg *>(op)->GetRegClass() == class_;
  }
  if (op->IsPhysicalReg()) {
    return static_cast<TargetPhysicalReg *>(op)->GetRegClass() == class_;
  }
  return false;
}

auto AArch64GraphColorRegAlloc::NodeOf(TargetOperand *op) -> NodeId {
  if (op->IsPhysicalReg()) {
    auto id = static_cast<TargetPhysicalReg *>(op)->GetID();
    int key = (static_cast<int>(class_) << 8) | id;
    auto it = phys_node_.find(key);
    if (it != phys_node_.end()) {
      return it->second;
    }
    NodeId node = static_cast<NodeId>(nodes_.size());
    nodes_.push_back(op);
    precolored_.push_back(true);
    color_.push_back(id);
    adj_list_.emplace_back();
    degree_.push_back(0);
    alias_.push_back(node);
    move_list_.emplace_back();
    spill_weight_.push_back(0.0);
    phys_node_[key] = node;
    return node;
  }
  auto it = vreg_node_.find(op);
  if (it != vreg_node_.end()) {
    return it->second;
  }
  NodeId node = static_cast<NodeId>(nodes_.size());
  nodes_.push_back(op);
  precolored_.push_back(false);
  color_.push_back(-1);
  adj_list_.emplace_back();
  degree_.push_back(0);
  alias_.push_back(node);
  move_list_.emplace_back();
  spill_weight_.push_back(0.0);
  vreg_node_[op] = node;
  return node;
}

auto AArch64GraphColorRegAlloc::DefsUses(TargetInst *inst, std::vector<NodeId> *defs, std::vector<NodeId> *uses)
    -> void {
  defs->clear();
  uses->clear();
  auto *ri = static_cast<AArch64Inst *>(inst);
  if (ri->GetOpcode() == A64Opcode::BL) {
    // A call clobbers every caller-saved register of the class: model each as a
    // def so values live across the call avoid them (or spill).
    for (auto id : reg_info_.AllocatableRegs(class_)) {
      if (reg_info_.IsCallerSaved(class_, id)) {
        defs->push_back(NodeOf(ctx_->PhysReg(class_, TargetRegSize::QWord, id)));
      }
    }
    // The argument registers are recorded as operands (after the callee symbol);
    // they are used by the call, so they stay live from their setup move.
    for (auto *op : inst->GetOperands()) {
      if (IsInClass(op)) {
        uses->push_back(NodeOf(op));
      }
    }
    return;
  }
  auto *res = inst->GetResult();
  if (res != nullptr && IsInClass(res)) {
    defs->push_back(NodeOf(res));
  }
  for (auto *op : inst->GetOperands()) {
    if (IsInClass(op)) {
      uses->push_back(NodeOf(op));
    }
  }
}

auto AArch64GraphColorRegAlloc::IsMove(TargetInst *inst, NodeId *dst, NodeId *src) -> bool {
  auto *ri = static_cast<AArch64Inst *>(inst);
  auto op = ri->GetOpcode();
  bool is_move = (class_ == TargetRegClass::General && op == A64Opcode::MOV) ||
                 (class_ == TargetRegClass::Float && op == A64Opcode::FMOV);
  if (!is_move) {
    return false;
  }
  auto *res = inst->GetResult();
  const auto &ops = inst->GetOperands();
  if (res == nullptr || ops.empty() || !IsInClass(res) || !IsInClass(ops[0])) {
    return false;
  }
  *dst = NodeOf(res);
  *src = NodeOf(ops[0]);
  return true;
}

auto AArch64GraphColorRegAlloc::BuildCFG() -> void {
  succ_.clear();
  std::unordered_map<std::string, TargetBlock *> by_label;
  for (auto *b : func_->GetBlocks()) {
    by_label[b->GetLabel()] = b;
  }
  const auto &blocks = func_->GetBlocks();
  for (size_t i = 0; i < blocks.size(); ++i) {
    auto *b = blocks[i];
    auto &succs = succ_[b];
    const auto &insts = b->GetInsts();
    bool ends_unconditionally = false;
    for (auto *inst : insts) {
      auto *ri = static_cast<AArch64Inst *>(inst);
      auto opc = ri->GetOpcode();
      // Branches / jumps carry their target label as an operand; map it back.
      if (opc == A64Opcode::B || opc == A64Opcode::B_COND || opc == A64Opcode::CBZ || opc == A64Opcode::CBNZ) {
        for (auto *op : inst->GetOperands()) {
          if (op->GetOpType() == TargetOperandType::Label) {
            auto it = by_label.find(static_cast<TargetLabel *>(op)->GetName());
            if (it != by_label.end() && std::find(succs.begin(), succs.end(), it->second) == succs.end()) {
              succs.push_back(it->second);
            }
          }
        }
      }
    }
    if (!insts.empty()) {
      auto last = static_cast<AArch64Inst *>(insts.back())->GetOpcode();
      ends_unconditionally = last == A64Opcode::B || last == A64Opcode::RET;
    }
    // Fall through to the next block when the block has no terminating transfer.
    if (!ends_unconditionally && i + 1 < blocks.size() &&
        std::find(succs.begin(), succs.end(), blocks[i + 1]) == succs.end()) {
      succs.push_back(blocks[i + 1]);
    }
  }
}

auto AArch64GraphColorRegAlloc::ComputeLoopDepth() -> void {
  // Loop-nesting depth per block, used to weight spill cost so a value used in a
  // hot loop body is far more expensive to spill than one used once at top level.
  // SysY lowers to a reducible CFG whose natural loops are contiguous block
  // ranges, so a back-edge b->h (successor h laid out at or before b) delimits a
  // loop spanning [index(h), index(b)]; a block's depth is the number of such
  // intervals covering it, which nests correctly for nested loops.
  loop_depth_.clear();
  const auto &blocks = func_->GetBlocks();
  std::unordered_map<TargetBlock *, size_t> index;
  for (size_t i = 0; i < blocks.size(); ++i) {
    index[blocks[i]] = i;
  }
  std::vector<int> depth(blocks.size(), 0);
  for (size_t i = 0; i < blocks.size(); ++i) {
    for (auto *s : succ_[blocks[i]]) {
      auto it = index.find(s);
      if (it != index.end() && it->second <= i) {  // back-edge: header .. latch
        for (size_t k = it->second; k <= i; ++k) {
          depth[k]++;
        }
      }
    }
  }
  for (size_t i = 0; i < blocks.size(); ++i) {
    loop_depth_[blocks[i]] = depth[i];
  }
}

auto AArch64GraphColorRegAlloc::Liveness() -> void {
  live_in_.clear();
  live_out_.clear();
  // Per-block upward-exposed uses and defs.
  std::unordered_map<TargetBlock *, std::unordered_set<NodeId>> use_b;
  std::unordered_map<TargetBlock *, std::unordered_set<NodeId>> def_b;
  std::vector<NodeId> defs;
  std::vector<NodeId> uses;
  for (auto *b : func_->GetBlocks()) {
    auto &ub = use_b[b];
    auto &db = def_b[b];
    for (auto *inst : b->GetInsts()) {
      DefsUses(inst, &defs, &uses);
      for (auto u : uses) {
        if (db.count(u) == 0) {
          ub.insert(u);
        }
      }
      for (auto d : defs) {
        db.insert(d);
      }
    }
    live_in_[b];
    live_out_[b];
  }
  // Backward dataflow to fixpoint over the reversed block order.
  const auto &blocks = func_->GetBlocks();
  bool changed = true;
  while (changed) {
    changed = false;
    for (auto it = blocks.rbegin(); it != blocks.rend(); ++it) {
      auto *b = *it;
      std::unordered_set<NodeId> out;
      for (auto *s : succ_[b]) {
        for (auto n : live_in_[s]) {
          out.insert(n);
        }
      }
      std::unordered_set<NodeId> in = use_b[b];
      for (auto n : out) {
        if (def_b[b].count(n) == 0) {
          in.insert(n);
        }
      }
      if (out.size() != live_out_[b].size() || in.size() != live_in_[b].size()) {
        changed = true;
      }
      live_out_[b] = std::move(out);
      live_in_[b] = std::move(in);
    }
  }
}

auto AArch64GraphColorRegAlloc::AddEdge(NodeId u, NodeId v) -> void {
  if (u == v) {
    return;
  }
  auto key = EdgeKey(u, v);
  if (adj_set_.count(key) != 0) {
    return;
  }
  adj_set_.insert(key);
  if (!precolored_[u]) {
    adj_list_[u].insert(v);
    degree_[u]++;
  }
  if (!precolored_[v]) {
    adj_list_[v].insert(u);
    degree_[v]++;
  }
}

auto AArch64GraphColorRegAlloc::Build() -> void {
  std::vector<NodeId> defs;
  std::vector<NodeId> uses;
  for (auto *b : func_->GetBlocks()) {
    // Weight each reference by loop depth (10x per level, capped) so that hot
    // loop-body values dominate the spill-cost ranking and stay in registers.
    int depth = loop_depth_.count(b) != 0 ? loop_depth_[b] : 0;
    double ref_weight = std::pow(10.0, std::min(depth, 6));
    std::unordered_set<NodeId> live = live_out_[b];
    const auto &insts = b->GetInsts();
    for (auto it = insts.rbegin(); it != insts.rend(); ++it) {
      auto *inst = *it;
      DefsUses(inst, &defs, &uses);
      for (auto n : defs) {
        spill_weight_[n] += ref_weight;
      }
      for (auto n : uses) {
        spill_weight_[n] += ref_weight;
      }
      NodeId mdst = 0;
      NodeId msrc = 0;
      if (IsMove(inst, &mdst, &msrc)) {
        live.erase(msrc);
        move_list_[mdst].push_back(inst);
        if (msrc != mdst) {
          move_list_[msrc].push_back(inst);
        }
        worklist_moves_.insert(inst);
      }
      for (auto d : defs) {
        live.insert(d);
      }
      for (auto d : defs) {
        for (auto l : live) {
          AddEdge(l, d);
        }
      }
      for (auto d : defs) {
        live.erase(d);
      }
      for (auto u : uses) {
        live.insert(u);
      }
    }
  }
}

auto AArch64GraphColorRegAlloc::Adjacent(NodeId n) -> std::vector<NodeId> {
  std::vector<NodeId> out;
  for (auto a : adj_list_[n]) {
    if (select_set_.count(a) == 0 && coalesced_nodes_.count(a) == 0) {
      out.push_back(a);
    }
  }
  return out;
}

auto AArch64GraphColorRegAlloc::NodeMoves(NodeId n) -> std::vector<TargetInst *> {
  std::vector<TargetInst *> out;
  for (auto *m : move_list_[n]) {
    if (active_moves_.count(m) != 0 || worklist_moves_.count(m) != 0) {
      out.push_back(m);
    }
  }
  return out;
}

auto AArch64GraphColorRegAlloc::MoveRelated(NodeId n) -> bool { return !NodeMoves(n).empty(); }

auto AArch64GraphColorRegAlloc::MakeWorklists() -> void {
  for (NodeId n = 0; n < static_cast<NodeId>(nodes_.size()); ++n) {
    if (precolored_[n]) {
      continue;
    }
    if (degree_[n] >= num_colors_) {
      spill_worklist_.insert(n);
    } else if (MoveRelated(n)) {
      freeze_worklist_.insert(n);
    } else {
      simplify_worklist_.insert(n);
    }
  }
}

auto AArch64GraphColorRegAlloc::EnableMoves(const std::vector<NodeId> &nodes) -> void {
  for (auto n : nodes) {
    for (auto *m : NodeMoves(n)) {
      if (active_moves_.count(m) != 0) {
        active_moves_.erase(m);
        worklist_moves_.insert(m);
      }
    }
  }
}

auto AArch64GraphColorRegAlloc::DecrementDegree(NodeId n) -> void {
  if (precolored_[n]) {
    return;
  }
  int d = degree_[n];
  degree_[n] = d - 1;
  if (d == num_colors_) {
    std::vector<NodeId> nodes = Adjacent(n);
    nodes.push_back(n);
    EnableMoves(nodes);
    spill_worklist_.erase(n);
    if (MoveRelated(n)) {
      freeze_worklist_.insert(n);
    } else {
      simplify_worklist_.insert(n);
    }
  }
}

auto AArch64GraphColorRegAlloc::Simplify() -> void {
  NodeId n = *simplify_worklist_.begin();
  simplify_worklist_.erase(simplify_worklist_.begin());
  select_stack_.push_back(n);
  select_set_.insert(n);
  for (auto m : Adjacent(n)) {
    DecrementDegree(m);
  }
}

auto AArch64GraphColorRegAlloc::GetAlias(NodeId n) -> NodeId {
  while (coalesced_nodes_.count(n) != 0) {
    n = alias_[n];
  }
  return n;
}

auto AArch64GraphColorRegAlloc::AddWorkList(NodeId u) -> void {
  if (!precolored_[u] && !MoveRelated(u) && degree_[u] < num_colors_) {
    freeze_worklist_.erase(u);
    simplify_worklist_.insert(u);
  }
}

auto AArch64GraphColorRegAlloc::GeorgeOK(NodeId t, NodeId r) -> bool {
  return degree_[t] < num_colors_ || precolored_[t] || adj_set_.count(EdgeKey(t, r)) != 0;
}

auto AArch64GraphColorRegAlloc::Conservative(const std::vector<NodeId> &nodes) -> bool {
  int k = 0;
  for (auto n : nodes) {
    if (degree_[n] >= num_colors_) {
      k++;
    }
  }
  return k < num_colors_;
}

auto AArch64GraphColorRegAlloc::Combine(NodeId u, NodeId v) -> void {
  if (freeze_worklist_.count(v) != 0) {
    freeze_worklist_.erase(v);
  } else {
    spill_worklist_.erase(v);
  }
  coalesced_nodes_.insert(v);
  alias_[v] = u;
  for (auto *m : move_list_[v]) {
    move_list_[u].push_back(m);
  }
  EnableMoves({v});
  for (auto t : Adjacent(v)) {
    AddEdge(t, u);
    DecrementDegree(t);
  }
  if (degree_[u] >= num_colors_ && freeze_worklist_.count(u) != 0) {
    freeze_worklist_.erase(u);
    spill_worklist_.insert(u);
  }
}

auto AArch64GraphColorRegAlloc::Coalesce() -> void {
  auto *m = *worklist_moves_.begin();
  worklist_moves_.erase(worklist_moves_.begin());
  NodeId x = 0;
  NodeId y = 0;
  IsMove(m, &x, &y);
  x = GetAlias(x);
  y = GetAlias(y);
  NodeId u = x;
  NodeId v = y;
  if (precolored_[y]) {
    u = y;
    v = x;
  }
  if (u == v) {
    coalesced_moves_.insert(m);
    AddWorkList(u);
  } else if (precolored_[v] || adj_set_.count(EdgeKey(u, v)) != 0 ||
             (precolored_[u] && allocatable_ids_.count(color_[u]) == 0)) {
    // Never coalesce a value into a reserved, non-allocatable physical register
    // (sp / zero / fp / ra): those cannot become value-carrying homes. Such a
    // move (e.g. `mv v, sp` for a frame address) is left in place instead.
    constrained_moves_.insert(m);
    AddWorkList(u);
    AddWorkList(v);
  } else {
    bool ok = false;
    if (precolored_[u]) {
      ok = true;
      for (auto t : Adjacent(v)) {
        if (!GeorgeOK(t, u)) {
          ok = false;
          break;
        }
      }
    } else {
      std::vector<NodeId> both = Adjacent(u);
      for (auto t : Adjacent(v)) {
        both.push_back(t);
      }
      ok = Conservative(both);
    }
    if (ok) {
      coalesced_moves_.insert(m);
      Combine(u, v);
      AddWorkList(u);
    } else {
      active_moves_.insert(m);
    }
  }
}

auto AArch64GraphColorRegAlloc::FreezeMoves(NodeId u) -> void {
  for (auto *m : NodeMoves(u)) {
    NodeId x = 0;
    NodeId y = 0;
    IsMove(m, &x, &y);
    NodeId v = 0;
    if (GetAlias(y) == GetAlias(u)) {
      v = GetAlias(x);
    } else {
      v = GetAlias(y);
    }
    active_moves_.erase(m);
    worklist_moves_.erase(m);
    frozen_moves_.insert(m);
    if (!precolored_[v] && NodeMoves(v).empty() && degree_[v] < num_colors_) {
      freeze_worklist_.erase(v);
      simplify_worklist_.insert(v);
    }
  }
}

auto AArch64GraphColorRegAlloc::Freeze() -> void {
  NodeId u = *freeze_worklist_.begin();
  freeze_worklist_.erase(freeze_worklist_.begin());
  simplify_worklist_.insert(u);
  FreezeMoves(u);
}

auto AArch64GraphColorRegAlloc::SpillCost(NodeId n) const -> double {
  if (spill_temps_.count(nodes_[n]) != 0) {
    return std::numeric_limits<double>::infinity();
  }
  int deg = degree_[n] > 0 ? degree_[n] : 1;
  return spill_weight_[n] / deg;
}

auto AArch64GraphColorRegAlloc::SelectSpill() -> void {
  NodeId best = -1;
  double best_cost = 0.0;
  for (auto n : spill_worklist_) {
    double cost = SpillCost(n);
    if (best == -1 || cost < best_cost) {
      best = n;
      best_cost = cost;
    }
  }
  spill_worklist_.erase(best);
  simplify_worklist_.insert(best);
  FreezeMoves(best);
}

auto AArch64GraphColorRegAlloc::AssignColors() -> void {
  const auto &allocatable = reg_info_.AllocatableRegs(class_);
  while (!select_stack_.empty()) {
    NodeId n = select_stack_.back();
    select_stack_.pop_back();
    select_set_.erase(n);
    std::unordered_set<int> ok(allocatable.begin(), allocatable.end());
    for (auto w : adj_list_[n]) {
      NodeId aw = GetAlias(w);
      if (colored_nodes_.count(aw) != 0 || precolored_[aw]) {
        ok.erase(color_[aw]);
      }
    }
    if (ok.empty()) {
      spilled_nodes_.insert(n);
    } else {
      colored_nodes_.insert(n);
      // Prefer allocatable order (caller-saved temps first) for determinism.
      for (auto id : allocatable) {
        if (ok.count(id) != 0) {
          color_[n] = id;
          break;
        }
      }
    }
  }
  if (spilled_nodes_.empty()) {
    for (auto n : coalesced_nodes_) {
      color_[n] = color_[GetAlias(n)];
    }
  }
}

auto AArch64GraphColorRegAlloc::SlotOf(TargetOperand *vreg) -> size_t {
  auto it = spill_slot_.find(vreg);
  if (it != spill_slot_.end()) {
    return it->second;
  }
  size_t slot = spill_count_++;
  spill_slot_[vreg] = slot;
  return slot;
}

// Materialize a (non-negative) frame offset into the x16 scratch and return it,
// then callers form [x16] to reach an out-of-range spill slot.
namespace {
auto MaterializeOffsetIntoX16(TargetContext *ctx, std::vector<TargetInst *> *out, long offset) -> TargetOperand * {
  auto o = static_cast<uint32_t>(offset);
  auto *addr = ctx->PhysReg(TargetRegClass::General, TargetRegSize::QWord, X16);
  auto *sp = ctx->PhysReg(TargetRegClass::General, TargetRegSize::QWord, SP);
  auto *mz = ctx->CreateInst<AArch64Inst>(A64Opcode::MOVZ);
  mz->SetResult(addr);
  mz->AddOperand(ctx->Imm(static_cast<int>(o & 0xFFFFU)));
  out->push_back(mz);
  if ((o >> 16U) != 0) {
    auto *mk = ctx->CreateInst<AArch64Inst>(A64Opcode::MOVK);
    mk->SetResult(addr);
    mk->AddOperand(addr);
    mk->AddOperand(ctx->Imm(static_cast<int>((o >> 16U) & 0xFFFFU)));
    mk->AddOperand(ctx->Imm(16));
    out->push_back(mk);
  }
  auto *add = ctx->CreateInst<AArch64Inst>(A64Opcode::ADD);
  add->SetResult(addr);
  add->AddOperand(sp);
  add->AddOperand(addr);
  out->push_back(add);
  return addr;
}
}  // namespace

auto AArch64GraphColorRegAlloc::EmitReload(std::vector<TargetInst *> *out, TargetOperand * /*vreg*/, TargetOperand *dst,
                                           size_t slot) -> void {
  // A single ldr reaches w/x/s correctly by the destination register's view.
  auto offset = static_cast<long>(spill_base_ + slot * 8);
  auto *sp = ctx_->PhysReg(TargetRegClass::General, TargetRegSize::QWord, SP);
  auto *ld = ctx_->CreateInst<AArch64Inst>(A64Opcode::LDR_OFF);
  ld->SetResult(dst);
  if (offset >= 0 && offset <= FRAME_OFFSET_MAX) {
    ld->AddOperand(sp);
    ld->AddOperand(ctx_->Imm(static_cast<int>(offset)));
  } else {
    ld->AddOperand(MaterializeOffsetIntoX16(ctx_, out, offset));
    ld->AddOperand(ctx_->Imm(0));
  }
  out->push_back(ld);
}

auto AArch64GraphColorRegAlloc::EmitSpill(std::vector<TargetInst *> *out, TargetOperand * /*vreg*/, TargetOperand *src,
                                          size_t slot) -> void {
  auto offset = static_cast<long>(spill_base_ + slot * 8);
  auto *sp = ctx_->PhysReg(TargetRegClass::General, TargetRegSize::QWord, SP);
  auto *st = ctx_->CreateInst<AArch64Inst>(A64Opcode::STR_OFF);
  st->AddOperand(src);
  if (offset >= 0 && offset <= FRAME_OFFSET_MAX) {
    st->AddOperand(sp);
    st->AddOperand(ctx_->Imm(static_cast<int>(offset)));
  } else {
    st->AddOperand(MaterializeOffsetIntoX16(ctx_, out, offset));
    st->AddOperand(ctx_->Imm(0));
  }
  out->push_back(st);
}

auto AArch64GraphColorRegAlloc::RewriteProgram() -> void {
  // Set of original vregs that must be spilled this round.
  std::unordered_set<TargetOperand *> spill_set;
  for (auto n : spilled_nodes_) {
    spill_set.insert(nodes_[n]);
    SlotOf(nodes_[n]);  // reserve the slot up front
  }
  for (auto *b : func_->GetBlocks()) {
    std::vector<TargetInst *> rewritten;
    for (auto *inst : b->GetInsts()) {
      std::vector<TargetInst *> before;
      std::vector<TargetInst *> after;
      std::unordered_map<TargetOperand *, TargetOperand *> local;
      // Uses: reload each distinct spilled operand into a fresh temp.
      for (auto *op : inst->GetOperands()) {
        if (op->IsVirtualReg() && spill_set.count(op) != 0 && local.count(op) == 0) {
          auto *v = static_cast<TargetVirtualReg *>(op);
          auto *tmp = ctx_->NewVirtualReg(v->GetRegClass(), v->GetRegSize());
          spill_temps_.insert(tmp);
          local[op] = tmp;
          EmitReload(&before, op, tmp, SlotOf(op));
          inst->ReplaceOperand(op, tmp);
        }
      }
      // Def: store the fresh temp back to the slot after the instruction.
      auto *res = inst->GetResult();
      if (res != nullptr && res->IsVirtualReg() && spill_set.count(res) != 0) {
        TargetOperand *tmp = nullptr;
        auto it = local.find(res);
        if (it != local.end()) {
          tmp = it->second;  // read-modify-write: reuse the reloaded temp
        } else {
          auto *v = static_cast<TargetVirtualReg *>(res);
          tmp = ctx_->NewVirtualReg(v->GetRegClass(), v->GetRegSize());
          spill_temps_.insert(tmp);
        }
        inst->ReplaceResult(tmp);
        EmitSpill(&after, res, tmp, SlotOf(res));
      }
      for (auto *i : before) {
        rewritten.push_back(i);
      }
      rewritten.push_back(inst);
      for (auto *i : after) {
        rewritten.push_back(i);
      }
    }
    b->SetInsts(rewritten);
  }
}

auto AArch64GraphColorRegAlloc::AllocateClass(TargetRegClass reg_class) -> void {
  class_ = reg_class;
  num_colors_ = static_cast<int>(reg_info_.NumAllocatable(reg_class));
  allocatable_ids_.clear();
  for (auto id : reg_info_.AllocatableRegs(reg_class)) {
    allocatable_ids_.insert(id);
  }

  BuildCFG();  // topology is stable across iterations, but rebuild is cheap
  ComputeLoopDepth();
  while (true) {
    // Reset per-iteration graph state.
    nodes_.clear();
    vreg_node_.clear();
    phys_node_.clear();
    precolored_.clear();
    color_.clear();
    adj_list_.clear();
    adj_set_.clear();
    degree_.clear();
    alias_.clear();
    move_list_.clear();
    spill_weight_.clear();
    simplify_worklist_.clear();
    freeze_worklist_.clear();
    spill_worklist_.clear();
    spilled_nodes_.clear();
    coalesced_nodes_.clear();
    colored_nodes_.clear();
    select_stack_.clear();
    select_set_.clear();
    worklist_moves_.clear();
    active_moves_.clear();
    frozen_moves_.clear();
    constrained_moves_.clear();
    coalesced_moves_.clear();

    Liveness();
    Build();
    MakeWorklists();
    while (!simplify_worklist_.empty() || !worklist_moves_.empty() || !freeze_worklist_.empty() ||
           !spill_worklist_.empty()) {
      if (!simplify_worklist_.empty()) {
        Simplify();
      } else if (!worklist_moves_.empty()) {
        Coalesce();
      } else if (!freeze_worklist_.empty()) {
        Freeze();
      } else {
        SelectSpill();
      }
    }
    AssignColors();
    if (spilled_nodes_.empty()) {
      break;
    }
    RewriteProgram();
  }

  // Record the coloring for this class; ApplyColors rewrites the program later.
  for (auto &[op, node] : vreg_node_) {
    int c = color_[node];
    if (c < 0) {
      continue;  // should not happen once allocation converges
    }
    assigned_[op] = static_cast<uint8_t>(c);
  }
}

auto AArch64GraphColorRegAlloc::ApplyColors() -> void {
  std::unordered_set<int> callee_seen;
  auto note_callee = [&](TargetRegClass cls, uint8_t id) {
    if (!reg_info_.IsCallerSaved(cls, id)) {
      int key = (static_cast<int>(cls) << 8) | id;
      if (callee_seen.insert(key).second) {
        used_callee_saved_.emplace_back(cls, id);
      }
    }
  };

  for (auto *b : func_->GetBlocks()) {
    std::vector<TargetInst *> kept;
    for (auto *inst : b->GetInsts()) {
      // Replace every virtual operand / result with its assigned physical reg.
      auto *res = inst->GetResult();
      if (res != nullptr && res->IsVirtualReg()) {
        auto *v = static_cast<TargetVirtualReg *>(res);
        auto it = assigned_.find(res);
        if (it != assigned_.end()) {
          auto *phys = ctx_->PhysReg(v->GetRegClass(), v->GetRegSize(), it->second);
          inst->ReplaceResult(phys);
          note_callee(v->GetRegClass(), it->second);
        }
      }
      for (auto *op : inst->GetOperands()) {
        if (op->IsVirtualReg()) {
          auto *v = static_cast<TargetVirtualReg *>(op);
          auto it = assigned_.find(op);
          if (it != assigned_.end()) {
            auto *phys = ctx_->PhysReg(v->GetRegClass(), v->GetRegSize(), it->second);
            inst->ReplaceOperand(op, phys);
            note_callee(v->GetRegClass(), it->second);
          }
        }
      }
      // Drop register-to-register moves that became a no-op (same phys register).
      auto *ri = static_cast<AArch64Inst *>(inst);
      auto opc = ri->GetOpcode();
      if (opc == A64Opcode::MOV || opc == A64Opcode::FMOV) {
        auto *r = inst->GetResult();
        const auto &ops = inst->GetOperands();
        if (r != nullptr && r->IsPhysicalReg() && !ops.empty() && ops[0]->IsPhysicalReg()) {
          auto *rp = static_cast<TargetPhysicalReg *>(r);
          auto *sp0 = static_cast<TargetPhysicalReg *>(ops[0]);
          if (rp->GetRegClass() == sp0->GetRegClass() && rp->GetID() == sp0->GetID()) {
            continue;  // omit the nop move
          }
        }
      }
      kept.push_back(inst);
    }
    b->SetInsts(kept);
  }
}

}  // namespace backend::targetir::aarch64
