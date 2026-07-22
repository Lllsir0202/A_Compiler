#include "backend/targets/riscv/pass/machinelicm.h"
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "backend/targets/riscv/inst/riscvinst.h"
#include "backend/targets/riscv/regs/riscvreg.h"
#include "backend/targets/utils/operand/targetliteral.h"
#include "backend/targets/utils/operand/targetreg.h"
#include "backend/targets/utils/operand/targetsymbol.h"
#include "backend/targets/utils/struct/targetblock.h"
#include "backend/targets/utils/struct/targetfunction.h"

namespace backend::targetir::riscv {

namespace {

// Pure register-to-register opcodes safe to speculate into a preheader: a
// deterministic function of their operands with no memory or control-flow
// effect and no trap. This mirrors the local-CSE purity set but deliberately
// excludes DIVW/REMW: those are safe from traps on RISC-V (division by zero is
// defined, not faulting) yet expensive, so speculating them out of a loop that
// might execute zero times could add work. Everything kept here is cheap.
auto IsHoistable(RVOpcode op) -> bool {
  switch (op) {
    case RVOpcode::ADDW:
    case RVOpcode::SUBW:
    case RVOpcode::MULW:
    case RVOpcode::ADD:
    case RVOpcode::SUB:
    case RVOpcode::MUL:
    case RVOpcode::AND:
    case RVOpcode::OR:
    case RVOpcode::XOR:
    case RVOpcode::SLLW:
    case RVOpcode::SRLW:
    case RVOpcode::SRAW:
    case RVOpcode::SLT:
    case RVOpcode::SLTU:
    case RVOpcode::ADDIW:
    case RVOpcode::ADDI:
    case RVOpcode::ANDI:
    case RVOpcode::ORI:
    case RVOpcode::XORI:
    case RVOpcode::SLLIW:
    case RVOpcode::SRLIW:
    case RVOpcode::SRAIW:
    case RVOpcode::SLLI:
    case RVOpcode::SRAI:
    case RVOpcode::SLTI:
    case RVOpcode::SLTIU:
    case RVOpcode::MV:
    case RVOpcode::NEGW:
    case RVOpcode::SEQZ:
    case RVOpcode::SNEZ:
    case RVOpcode::SEXT_W:
    case RVOpcode::LI:
    case RVOpcode::LA:
      return true;
    default:
      return false;
  }
}

// A trivially rematerializable op produces its value from no register inputs
// (an immediate or a symbol address). Hoisting one adds a value that stays live
// across the whole loop without letting any operand die in exchange, so it is
// pure register pressure: worth doing only while the loop has spare registers.
// Multi-input arithmetic, by contrast, can let its (now loop-dead) source
// operands be freed, so it is far less likely to raise pressure.
auto IsRematerializable(RVOpcode op) -> bool { return op == RVOpcode::LI || op == RVOpcode::LA; }

// A natural loop: its header, the set of body blocks (including the header),
// and its unique out-of-loop preheader (nullptr if none / not eligible).
struct Loop {
  TargetBlock *header = nullptr;
  TargetBlock *preheader = nullptr;
  std::unordered_set<TargetBlock *> body;
};

class MachineLICMImpl {
 public:
  explicit MachineLICMImpl(TargetFunction *func) : func_(func) {}

  auto Run() -> bool {
    const auto &blocks = func_->GetBlocks();
    if (blocks.size() < 2) {
      return false;
    }
    BuildCFG();
    BuildDefCounts();
    ComputeDominators();
    auto loops = FindLoops();
    bool changed = false;
    for (auto &loop : loops) {
      if (loop.preheader != nullptr) {
        changed |= HoistLoop(loop);
      }
    }
    return changed;
  }

 private:
  auto BuildCFG() -> void {
    const auto &blocks = func_->GetBlocks();
    index_.clear();
    succ_.clear();
    pred_.clear();
    for (size_t i = 0; i < blocks.size(); ++i) {
      index_[blocks[i]] = i;
    }
    std::unordered_map<std::string, TargetBlock *> by_label;
    for (auto *b : blocks) {
      by_label[b->GetLabel()] = b;
    }
    for (size_t i = 0; i < blocks.size(); ++i) {
      auto *b = blocks[i];
      auto &succs = succ_[b];
      const auto &insts = b->GetInsts();
      bool ends_unconditionally = false;
      for (auto *inst : insts) {
        auto opc = static_cast<RISCVInst *>(inst)->GetOpcode();
        if (opc == RVOpcode::J || opc == RVOpcode::BEQ || opc == RVOpcode::BNE || opc == RVOpcode::BLT ||
            opc == RVOpcode::BGE || opc == RVOpcode::BLTU || opc == RVOpcode::BGEU) {
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
        auto last = static_cast<RISCVInst *>(insts.back())->GetOpcode();
        ends_unconditionally = last == RVOpcode::J || last == RVOpcode::RET;
      }
      if (!ends_unconditionally && i + 1 < blocks.size() &&
          std::find(succs.begin(), succs.end(), blocks[i + 1]) == succs.end()) {
        succs.push_back(blocks[i + 1]);
      }
    }
    for (auto *b : blocks) {
      for (auto *s : succ_[b]) {
        pred_[s].push_back(b);
      }
    }
  }

  // Count function-wide definitions per virtual register id. Backend vregs are
  // not SSA (loop phis / accumulators are multiply defined), so only a single-def
  // result is safe to relocate and its operands are stable values.
  auto BuildDefCounts() -> void {
    def_count_.clear();
    for (auto *b : func_->GetBlocks()) {
      for (auto *inst : b->GetInsts()) {
        auto *res = inst->GetResult();
        if (res != nullptr && res->IsVirtualReg()) {
          def_count_[static_cast<TargetVirtualReg *>(res)->GetID()]++;
        }
      }
    }
  }

  // Iterative dominators over the layout order (entry = first block). Standard
  // fixpoint on a reducible CFG; cheap for the small functions here.
  auto ComputeDominators() -> void {
    const auto &blocks = func_->GetBlocks();
    size_t n = blocks.size();
    dom_.assign(n, {});
    std::vector<std::unordered_set<size_t>> dom(n);
    std::unordered_set<size_t> all;
    for (size_t i = 0; i < n; ++i) {
      all.insert(i);
    }
    dom[0] = {0};
    for (size_t i = 1; i < n; ++i) {
      dom[i] = all;
    }
    bool changed = true;
    while (changed) {
      changed = false;
      for (size_t i = 1; i < n; ++i) {
        std::unordered_set<size_t> inter;
        bool first = true;
        for (auto *p : pred_[blocks[i]]) {
          size_t pi = index_[p];
          if (first) {
            inter = dom[pi];
            first = false;
          } else {
            std::unordered_set<size_t> next;
            for (size_t d : inter) {
              if (dom[pi].count(d) != 0) {
                next.insert(d);
              }
            }
            inter = std::move(next);
          }
        }
        inter.insert(i);
        if (inter != dom[i]) {
          dom[i] = std::move(inter);
          changed = true;
        }
      }
    }
    dom_ = std::move(dom);
  }

  auto Dominates(size_t a, size_t b) const -> bool { return dom_[b].count(a) != 0; }

  // Natural loops: for every edge b->h with h dominating b (a back-edge), the
  // loop body is h plus every block that can reach b without going through h.
  auto FindLoops() -> std::vector<Loop> {
    const auto &blocks = func_->GetBlocks();
    std::vector<Loop> loops;
    for (auto *b : blocks) {
      size_t bi = index_[b];
      for (auto *h : succ_[b]) {
        size_t hi = index_[h];
        if (!Dominates(hi, bi)) {
          continue;  // not a back-edge
        }
        Loop loop;
        loop.header = h;
        loop.body.insert(h);
        std::vector<TargetBlock *> work;
        if (b != h) {
          loop.body.insert(b);
          work.push_back(b);
        }
        while (!work.empty()) {
          auto *cur = work.back();
          work.pop_back();
          for (auto *p : pred_[cur]) {
            if (loop.body.insert(p).second) {
              work.push_back(p);
            }
          }
        }
        loop.preheader = FindPreheader(loop);
        loops.push_back(std::move(loop));
      }
    }
    return loops;
  }

  // A usable preheader is the single loop-external predecessor of the header,
  // and it must dominate the header (so hoisted code always runs before the
  // loop). Otherwise there is no safe single insertion point and we skip.
  auto FindPreheader(const Loop &loop) -> TargetBlock * {
    TargetBlock *ph = nullptr;
    for (auto *p : pred_[loop.header]) {
      if (loop.body.count(p) != 0) {
        continue;  // in-loop predecessor (the latch)
      }
      if (ph != nullptr) {
        return nullptr;  // more than one entry edge: no unique preheader
      }
      ph = p;
    }
    if (ph == nullptr) {
      return nullptr;
    }
    return Dominates(index_[ph], index_[loop.header]) ? ph : nullptr;
  }

  auto IsX0(TargetOperand *op) const -> bool {
    if (op == nullptr || !op->IsPhysicalReg()) {
      return false;
    }
    auto *reg = static_cast<TargetPhysicalReg *>(op);
    return reg->GetRegClass() == TargetRegClass::General && reg->GetID() == static_cast<uint8_t>(GPR::Zero);
  }

  // An operand is loop-invariant if it is a constant/symbol/label/x0, or a
  // virtual register whose (single) definition lies outside the loop body, or a
  // register already scheduled for hoisting this round.
  auto OperandInvariant(TargetOperand *op, const std::unordered_set<size_t> &hoisted) const -> bool {
    switch (op->GetOpType()) {
      case TargetOperandType::Immediate:
      case TargetOperandType::Symbol:
      case TargetOperandType::Label:
        return true;
      case TargetOperandType::PhysicalReg:
        return IsX0(op);  // any other physical reg (sp/fp/args) is not proven invariant
      case TargetOperandType::VirtualReg: {
        size_t id = static_cast<TargetVirtualReg *>(op)->GetID();
        if (hoisted.count(id) != 0) {
          return true;
        }
        auto it = def_in_loop_.find(id);
        return it == def_in_loop_.end();  // no definition inside the loop body
      }
      default:
        return false;
    }
  }

  static auto ClassOf(TargetOperand *op) -> TargetRegClass {
    return static_cast<TargetVirtualReg *>(op)->GetRegClass();
  }

  // Estimate the loop body's peak register pressure per class: the maximum
  // number of virtual registers simultaneously live at any point inside the
  // loop. Computed with a local backward liveness sweep over the body blocks; a
  // value is treated as live throughout the body if it is used in the loop but
  // defined outside (loop-invariant inputs) or crosses a back-edge. This is an
  // approximation -- it ignores physical registers and the exact allocator
  // interference graph -- but it is a sound upper-bound signal for deciding
  // whether the loop has spare registers to absorb a hoisted value.
  auto PeakPressure(const Loop &loop, TargetRegClass cls) -> int {
    // live-out of a block = union of live-in of successors that are in the loop,
    // plus values used later in the loop. We iterate to a fixpoint over the body.
    std::unordered_map<TargetBlock *, std::unordered_set<size_t>> live_in;
    auto in_class = [&](TargetOperand *op) { return op != nullptr && op->IsVirtualReg() && ClassOf(op) == cls; };
    bool changed = true;
    int guard = 0;
    while (changed && guard++ < 64) {
      changed = false;
      for (auto it = loop.body.begin(); it != loop.body.end(); ++it) {
        auto *b = *it;
        std::unordered_set<size_t> live;
        for (auto *s : succ_[b]) {
          if (loop.body.count(s) != 0) {
            auto lit = live_in.find(s);
            if (lit != live_in.end()) {
              live.insert(lit->second.begin(), lit->second.end());
            }
          }
        }
        // Walk instructions backward: def kills, use makes live.
        const auto &insts = b->GetInsts();
        for (auto rit = insts.rbegin(); rit != insts.rend(); ++rit) {
          auto *res = (*rit)->GetResult();
          if (in_class(res)) {
            live.erase(static_cast<TargetVirtualReg *>(res)->GetID());
          }
          for (auto *op : (*rit)->GetOperands()) {
            if (in_class(op)) {
              live.insert(static_cast<TargetVirtualReg *>(op)->GetID());
            }
          }
        }
        auto &slot = live_in[b];
        if (slot != live) {
          slot = std::move(live);
          changed = true;
        }
      }
    }
    // Second pass: replay each block backward from its live-out, tracking the
    // running live set size and recording the maximum seen anywhere in the body.
    int peak = 0;
    for (auto *b : loop.body) {
      std::unordered_set<size_t> live;
      for (auto *s : succ_[b]) {
        if (loop.body.count(s) != 0) {
          auto lit = live_in.find(s);
          if (lit != live_in.end()) {
            live.insert(lit->second.begin(), lit->second.end());
          }
        }
      }
      peak = std::max(peak, static_cast<int>(live.size()));
      const auto &insts = b->GetInsts();
      for (auto rit = insts.rbegin(); rit != insts.rend(); ++rit) {
        auto *res = (*rit)->GetResult();
        if (in_class(res)) {
          live.erase(static_cast<TargetVirtualReg *>(res)->GetID());
        }
        for (auto *op : (*rit)->GetOperands()) {
          if (in_class(op)) {
            live.insert(static_cast<TargetVirtualReg *>(op)->GetID());
          }
        }
        peak = std::max(peak, static_cast<int>(live.size()));
      }
    }
    return peak;
  }

  auto HoistLoop(const Loop &loop) -> bool {
    // Which virtual-register ids are defined anywhere in the loop body. Used to
    // decide operand invariance without per-instruction scans.
    def_in_loop_.clear();
    for (auto *b : loop.body) {
      for (auto *inst : b->GetInsts()) {
        auto *res = inst->GetResult();
        if (res != nullptr && res->IsVirtualReg()) {
          def_in_loop_.insert(static_cast<TargetVirtualReg *>(res)->GetID());
        }
      }
    }

    // Register-pressure budget for rematerializable (li/la) hoists, per class.
    // Each such hoist adds one value live across the whole loop without freeing
    // an operand, so we only allow them while the loop's peak pressure leaves
    // headroom below the allocatable file (with a margin for the allocator's own
    // temporaries and physical-register interference the estimate ignores).
    // Two limits gate rematerializable hoists, and the smaller wins per class.
    // The liveness estimate below is an approximation -- it does not model the
    // physical-register interference (incoming arguments, call clobbers, the
    // multi-temp division-magic sequence) that the real allocator sees -- so it
    // can be optimistic. A small absolute cap backstops that: the profitable
    // constant hoists observed in practice cluster at a handful per loop (a hot
    // loop reuses a bound or a modulus), while the regressions came from loops
    // that hoisted many constants at once and overflowed the register file.
    constexpr int kGprAllocatable = 25;
    constexpr int kFprAllocatable = 32;
    constexpr int kMargin = 4;
    constexpr int kRematCap = 3;
    int gpr_budget =
        std::min(kRematCap, std::max(0, kGprAllocatable - kMargin - PeakPressure(loop, TargetRegClass::General)));
    int fpr_budget =
        std::min(kRematCap, std::max(0, kFprAllocatable - kMargin - PeakPressure(loop, TargetRegClass::Float)));

    // The pass manager re-runs the whole pipeline to a fixpoint, so a naive
    // per-run cap would be defeated: each run would hoist another batch of
    // constants that a previous run already left in the preheader. Charge those
    // prior hoists against the budget by counting rematerializable definitions
    // already sitting in the preheader whose value is used inside the loop.
    for (auto *inst : loop.preheader->GetInsts()) {
      auto *res = inst->GetResult();
      if (res == nullptr || !res->IsVirtualReg() || !IsRematerializable(static_cast<RISCVInst *>(inst)->GetOpcode())) {
        continue;
      }
      size_t id = static_cast<TargetVirtualReg *>(res)->GetID();
      if (def_in_loop_.count(id) != 0) {
        continue;  // also defined in-loop: not a loop-invariant preheader value
      }
      bool used_in_loop = false;
      for (auto *b : loop.body) {
        for (auto *bi : b->GetInsts()) {
          for (auto *op : bi->GetOperands()) {
            if (op->IsVirtualReg() && static_cast<TargetVirtualReg *>(op)->GetID() == id) {
              used_in_loop = true;
              break;
            }
          }
          if (used_in_loop) {
            break;
          }
        }
        if (used_in_loop) {
          break;
        }
      }
      if (used_in_loop) {
        int &budget = ClassOf(res) == TargetRegClass::Float ? fpr_budget : gpr_budget;
        if (budget > 0) {
          --budget;
        }
      }
    }

    // Visit body blocks in layout order so a hoisted producer is seen before its
    // consumer; iterate to a fixpoint so chains (slli -> add) hoist together.
    std::vector<TargetBlock *> ordered;
    for (auto *b : func_->GetBlocks()) {
      if (loop.body.count(b) != 0) {
        ordered.push_back(b);
      }
    }

    std::unordered_set<size_t> hoisted;                           // vreg ids already moved
    std::vector<std::pair<TargetBlock *, TargetInst *>> to_move;  // in hoist order
    std::unordered_set<TargetInst *> moved;

    bool progress = true;
    while (progress) {
      progress = false;
      for (auto *b : ordered) {
        for (auto *inst : b->GetInsts()) {
          if (moved.count(inst) != 0) {
            continue;
          }
          auto *ri = static_cast<RISCVInst *>(inst);
          if (!IsHoistable(ri->GetOpcode())) {
            continue;
          }
          auto *res = inst->GetResult();
          if (res == nullptr || !res->IsVirtualReg()) {
            continue;
          }
          size_t rid = static_cast<TargetVirtualReg *>(res)->GetID();
          auto dc = def_count_.find(rid);
          if (dc == def_count_.end() || dc->second != 1) {
            continue;  // multiply-defined result: not safe to relocate
          }
          bool invariant = true;
          for (auto *op : inst->GetOperands()) {
            if (!OperandInvariant(op, hoisted)) {
              invariant = false;
              break;
            }
          }
          if (!invariant) {
            continue;
          }
          // A rematerializable hoist adds pure loop-carried pressure; spend from
          // the per-class budget and skip once exhausted, leaving those constants
          // to be recomputed in the body rather than provoking a hot-loop spill.
          if (IsRematerializable(ri->GetOpcode())) {
            int &budget = ClassOf(res) == TargetRegClass::Float ? fpr_budget : gpr_budget;
            if (budget <= 0) {
              continue;
            }
            --budget;
          }
          hoisted.insert(rid);
          moved.insert(inst);
          to_move.emplace_back(b, inst);
          progress = true;
        }
      }
    }

    if (to_move.empty()) {
      return false;
    }

    // Remove moved instructions from their body blocks.
    for (auto *b : ordered) {
      const auto &insts = b->GetInsts();
      std::vector<TargetInst *> kept;
      kept.reserve(insts.size());
      for (auto *inst : insts) {
        if (moved.count(inst) == 0) {
          kept.push_back(inst);
        }
      }
      if (kept.size() != insts.size()) {
        b->SetInsts(std::move(kept));
      }
    }

    // Splice into the preheader before its terminating transfer, preserving the
    // discovered hoist order (producers before consumers).
    auto *ph = loop.preheader;
    std::vector<TargetInst *> ph_insts = ph->GetInsts();
    size_t insert_at = ph_insts.size();
    while (insert_at > 0) {
      auto opc = static_cast<RISCVInst *>(ph_insts[insert_at - 1])->GetOpcode();
      bool is_transfer = opc == RVOpcode::J || opc == RVOpcode::RET || opc == RVOpcode::BEQ || opc == RVOpcode::BNE ||
                         opc == RVOpcode::BLT || opc == RVOpcode::BGE || opc == RVOpcode::BLTU || opc == RVOpcode::BGEU;
      if (!is_transfer) {
        break;
      }
      --insert_at;
    }
    std::vector<TargetInst *> hoisted_seq;
    hoisted_seq.reserve(to_move.size());
    for (auto &entry : to_move) {
      hoisted_seq.push_back(entry.second);
    }
    ph_insts.insert(ph_insts.begin() + static_cast<long>(insert_at), hoisted_seq.begin(), hoisted_seq.end());
    ph->SetInsts(std::move(ph_insts));
    return true;
  }

  TargetFunction *func_;
  std::unordered_map<TargetBlock *, size_t> index_;
  std::unordered_map<TargetBlock *, std::vector<TargetBlock *>> succ_;
  std::unordered_map<TargetBlock *, std::vector<TargetBlock *>> pred_;
  std::vector<std::unordered_set<size_t>> dom_;  // dom_[i] = blocks dominating block i
  std::unordered_map<size_t, int> def_count_;    // vreg id -> definition count (function-wide)
  std::unordered_set<size_t> def_in_loop_;       // vreg ids defined in the current loop body
};

}  // namespace

auto MachineLICM::RunOnFunction(TargetFunction *func) -> bool {
  MachineLICMImpl impl(func);
  return impl.Run();
}

}  // namespace backend::targetir::riscv
