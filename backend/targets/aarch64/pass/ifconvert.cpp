#include "backend/targets/aarch64/pass/ifconvert.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "backend/targets/aarch64/inst/aarch64inst.h"
#include "backend/targets/utils/operand/targetreg.h"
#include "backend/targets/utils/operand/targetsymbol.h"
#include "backend/targets/utils/struct/targetblock.h"
#include "backend/targets/utils/struct/targetfunction.h"

namespace backend::targetir::aarch64 {

namespace {
constexpr size_t kMaxHoist = 6;    // total pure ops speculated across both arms
constexpr size_t kMaxOutputs = 3;  // csel count

// Cheap, side-effect-free, non-faulting, non-flag-setting ops that are safe to
// execute unconditionally when hoisted out of an arm. Loads (may fault), stores,
// calls, cmp/cset (flags), div (kept out to keep arms cheap) and branches are absent.
auto IsHoistablePure(A64Opcode op) -> bool {
  switch (op) {
    case A64Opcode::ADD:
    case A64Opcode::SUB:
    case A64Opcode::MUL:
    case A64Opcode::AND:
    case A64Opcode::ORR:
    case A64Opcode::EOR:
    case A64Opcode::LSLV:
    case A64Opcode::LSRV:
    case A64Opcode::ASRV:
    case A64Opcode::MSUB:
    case A64Opcode::MADD:
    case A64Opcode::ADD_I:
    case A64Opcode::SUB_I:
    case A64Opcode::AND_I:
    case A64Opcode::ORR_I:
    case A64Opcode::EOR_I:
    case A64Opcode::LSL_I:
    case A64Opcode::LSR_I:
    case A64Opcode::ASR_I:
    case A64Opcode::MOV:
    case A64Opcode::MOVZ:
    case A64Opcode::MOVN:
    case A64Opcode::SXTW:
    case A64Opcode::FADD:
    case A64Opcode::FSUB:
    case A64Opcode::FMUL:
    case A64Opcode::FMOV:
    case A64Opcode::SCVTF:
    case A64Opcode::FCVTZS:
    case A64Opcode::ADD_SHIFT_LSL:
    case A64Opcode::ADD_SHIFT_LSR:
    case A64Opcode::ADD_SHIFT_ASR:
    case A64Opcode::SUB_SHIFT_LSL:
    case A64Opcode::SUB_SHIFT_LSR:
    case A64Opcode::SUB_SHIFT_ASR:
    case A64Opcode::ADD_EXT_SXTW:
    case A64Opcode::SUB_EXT_SXTW:
    case A64Opcode::ADD_I_LSL12:
    case A64Opcode::SUB_I_LSL12:
      return true;
    default:
      return false;
  }
}

auto IsUncondBranch(A64Opcode op) -> bool { return op == A64Opcode::B; }
auto IsFlagSetter(A64Opcode op) -> bool {
  return op == A64Opcode::CMP || op == A64Opcode::CMP_I || op == A64Opcode::FCMP;
}

// The label a B / B_COND transfers to, or empty for a non-label branch.
auto BranchTargetLabel(TargetInst *inst) -> std::string {
  for (auto *op : inst->GetOperands()) {
    if (op->GetOpType() == TargetOperandType::Label) {
      return static_cast<TargetLabel *>(op)->GetName();
    }
  }
  return "";
}

auto VId(TargetOperand *op) -> size_t { return static_cast<TargetVirtualReg *>(op)->GetID(); }

// One recognized diamond ready to be rewritten in place.
struct Diamond {
  TargetBlock *h_;
  TargetBlock *t_;
  TargetBlock *f_;
  A64Cond cond_;
  TargetInst *branch_to_m_;            // reused `b M` terminator
  std::vector<TargetInst *> hoist_;    // pure ops of both arms, in order
  std::vector<TargetOperand *> outs_;  // merged phi vregs
  std::vector<TargetOperand *> src_true_;
  std::vector<TargetOperand *> src_false_;
};
}  // namespace

auto AArch64IfConvert::RunOnFunction(TargetFunction *func) -> bool {
  const auto &blocks = func->GetBlocks();
  std::unordered_map<std::string, TargetBlock *> by_label;
  for (auto *b : blocks) {
    by_label[b->GetLabel()] = b;
  }

  // Successors (explicit branch targets + fall-through), then predecessors.
  std::unordered_map<TargetBlock *, std::vector<TargetBlock *>> preds;
  for (auto *b : blocks) {
    preds[b];
  }
  for (size_t i = 0; i < blocks.size(); ++i) {
    auto *b = blocks[i];
    const auto &insts = b->GetInsts();
    bool ends_uncond = false;
    for (auto *inst : insts) {
      auto op = static_cast<AArch64Inst *>(inst)->GetOpcode();
      if (op == A64Opcode::B || op == A64Opcode::B_COND || op == A64Opcode::CBZ || op == A64Opcode::CBNZ) {
        auto it = by_label.find(BranchTargetLabel(inst));
        if (it != by_label.end()) {
          preds[it->second].push_back(b);
        }
      }
      if (op == A64Opcode::B || op == A64Opcode::RET) {
        ends_uncond = true;
      }
    }
    if (!ends_uncond && i + 1 < blocks.size()) {
      preds[blocks[i + 1]].push_back(b);
    }
  }

  // Blocks in which each virtual register is *used* (appears as an operand).
  std::unordered_map<size_t, std::unordered_set<TargetBlock *>> use_blocks;
  for (auto *b : blocks) {
    for (auto *inst : b->GetInsts()) {
      for (auto *op : inst->GetOperands()) {
        if (op->IsVirtualReg()) {
          use_blocks[VId(op)].insert(b);
        }
      }
    }
  }
  auto used_outside = [&](TargetOperand *reg, TargetBlock *arm) {
    auto it = use_blocks.find(VId(reg));
    if (it == use_blocks.end()) {
      return false;
    }
    for (auto *ub : it->second) {
      if (ub != arm) {
        return true;
      }
    }
    return false;
  };

  // Analyze one arm. Fills `merge`, the hoistable pure ops, and the phi outputs
  // (each an output vreg + the value this arm supplies). Returns false on any op
  // that is not a clean phi copy or a hoistable pure computation.
  auto analyze = [&](TargetBlock *arm, TargetBlock *h, TargetBlock **merge, std::vector<TargetInst *> *hoist,
                     std::vector<TargetOperand *> *outs, std::vector<TargetOperand *> *srcs) -> bool {
    if (preds[arm].size() != 1 || preds[arm][0] != h) {
      return false;  // must be reached only from the header
    }
    const auto &insts = arm->GetInsts();
    if (insts.empty()) {
      return false;
    }
    auto *term = static_cast<AArch64Inst *>(insts.back());
    if (!IsUncondBranch(term->GetOpcode())) {
      return false;
    }
    auto it = by_label.find(BranchTargetLabel(term));
    if (it == by_label.end()) {
      return false;
    }
    *merge = it->second;
    for (size_t i = 0; i + 1 < insts.size(); ++i) {
      auto *inst = static_cast<AArch64Inst *>(insts[i]);
      auto *res = inst->GetResult();
      // A register move whose result is consumed after the merge is a phi copy.
      if (inst->GetOpcode() == A64Opcode::MOV && res != nullptr && res->IsVirtualReg() && used_outside(res, arm)) {
        const auto &ops = inst->GetOperands();
        if (ops.size() != 1 || !ops[0]->IsVirtualReg()) {
          return false;  // csel needs a register source on both sides
        }
        outs->push_back(res);
        srcs->push_back(ops[0]);
        continue;
      }
      // Otherwise it must be a speculatable pure op that is purely local to the arm.
      if (!IsHoistablePure(inst->GetOpcode())) {
        return false;
      }
      if (res != nullptr && res->IsVirtualReg() && used_outside(res, arm)) {
        return false;  // an output not produced by a phi copy: out of scope for v1
      }
      hoist->push_back(inst);
    }
    return true;
  };

  std::vector<Diamond> diamonds;
  std::unordered_set<TargetBlock *> claimed;
  for (auto *h : blocks) {
    const auto &insts = h->GetInsts();
    if (insts.size() < 3) {
      continue;
    }
    auto *last = static_cast<AArch64Inst *>(insts.back());
    auto *prev = static_cast<AArch64Inst *>(insts[insts.size() - 2]);
    auto *prev2 = static_cast<AArch64Inst *>(insts[insts.size() - 3]);
    // Shape: [... cmp][b.<cc> T][b F]
    if (last->GetOpcode() != A64Opcode::B || prev->GetOpcode() != A64Opcode::B_COND ||
        !IsFlagSetter(prev2->GetOpcode())) {
      continue;
    }
    auto t_it = by_label.find(BranchTargetLabel(prev));
    auto f_it = by_label.find(BranchTargetLabel(last));
    if (t_it == by_label.end() || f_it == by_label.end()) {
      continue;
    }
    TargetBlock *t = t_it->second;
    TargetBlock *f = f_it->second;
    if (t == f || t == h || f == h) {
      continue;
    }

    TargetBlock *mt = nullptr;
    TargetBlock *mf = nullptr;
    std::vector<TargetInst *> hoist_t;
    std::vector<TargetInst *> hoist_f;
    std::vector<TargetOperand *> out_t;
    std::vector<TargetOperand *> src_t;
    std::vector<TargetOperand *> out_f;
    std::vector<TargetOperand *> src_f;
    if (!analyze(t, h, &mt, &hoist_t, &out_t, &src_t)) {
      continue;
    }
    if (!analyze(f, h, &mf, &hoist_f, &out_f, &src_f)) {
      continue;
    }
    if (mt != mf || mt == t || mt == f || mt == h) {
      continue;
    }
    if (out_t.empty() || out_t.size() > kMaxOutputs || out_t.size() != out_f.size()) {
      continue;
    }
    if (hoist_t.size() + hoist_f.size() > kMaxHoist) {
      continue;
    }
    // Match the two arms' outputs by vreg id; build the false-side source per output.
    std::unordered_map<size_t, TargetOperand *> f_src;
    for (size_t i = 0; i < out_f.size(); ++i) {
      f_src[VId(out_f[i])] = src_f[i];
    }
    bool ok = true;
    std::vector<TargetOperand *> src_false_ordered;
    for (auto *v : out_t) {
      auto it = f_src.find(VId(v));
      if (it == f_src.end()) {
        ok = false;
        break;
      }
      src_false_ordered.push_back(it->second);
    }
    if (!ok) {
      continue;
    }
    if (claimed.count(h) != 0 || claimed.count(t) != 0 || claimed.count(f) != 0 || claimed.count(mt) != 0) {
      continue;
    }
    claimed.insert(h);
    claimed.insert(t);
    claimed.insert(f);
    claimed.insert(mt);

    Diamond d;
    d.h_ = h;
    d.t_ = t;
    d.f_ = f;
    d.cond_ = prev->GetCond();
    d.branch_to_m_ = t->GetInsts().back();  // reuse `b M`
    d.hoist_ = hoist_t;
    d.hoist_.insert(d.hoist_.end(), hoist_f.begin(), hoist_f.end());
    d.outs_ = out_t;
    d.src_true_ = src_t;
    d.src_false_ = src_false_ordered;
    diamonds.push_back(std::move(d));
  }

  if (diamonds.empty()) {
    return false;
  }

  std::unordered_set<TargetBlock *> remove;
  for (auto &d : diamonds) {
    // Rewrite the header: drop the two branches, keep everything else (incl. cmp),
    // append the hoisted pure ops, then a csel per merged value, then `b M`.
    std::vector<TargetInst *> kept;
    const auto &hi = d.h_->GetInsts();
    kept.reserve(hi.size() + d.hoist_.size() + d.outs_.size());
    for (size_t i = 0; i + 2 < hi.size(); ++i) {  // all but the trailing b.cond and b
      kept.push_back(hi[i]);
    }
    for (auto *op : d.hoist_) {
      kept.push_back(op);
    }
    for (size_t i = 0; i < d.outs_.size(); ++i) {
      auto *csel = new AArch64Inst(A64Opcode::CSEL);
      csel->SetResult(d.outs_[i]);
      csel->AddOperand(d.src_true_[i]);
      csel->AddOperand(d.src_false_[i]);
      csel->SetCond(d.cond_);
      kept.push_back(csel);
    }
    kept.push_back(d.branch_to_m_);
    d.h_->SetInsts(kept);
    remove.insert(d.t_);
    remove.insert(d.f_);
  }

  std::vector<TargetBlock *> new_blocks;
  new_blocks.reserve(blocks.size());
  for (auto *b : blocks) {
    if (remove.count(b) == 0) {
      new_blocks.push_back(b);
    }
  }
  func->SetBlocks(new_blocks);
  return true;
}

}  // namespace backend::targetir::aarch64
