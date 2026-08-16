#include "backend/targets/aarch64/pass/machinelicm.h"
#include <algorithm>
#include <cstdlib>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "backend/targets/aarch64/inst/aarch64inst.h"
#include "backend/targets/utils/operand/targetliteral.h"
#include "backend/targets/utils/operand/targetreg.h"
#include "backend/targets/utils/operand/targetsymbol.h"
#include "backend/targets/utils/struct/targetblock.h"
#include "backend/targets/utils/struct/targetfunction.h"

namespace backend::targetir::aarch64 {
namespace {

auto IsHoistable(A64Opcode op) -> bool {
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
    case A64Opcode::ADRP:
    case A64Opcode::ADD_LO12:
    case A64Opcode::FADD:
    case A64Opcode::FSUB:
    case A64Opcode::FMUL:
    case A64Opcode::FMOV:
    case A64Opcode::FMOV_TO_GPR:
    case A64Opcode::FMOV_FROM_GPR:
    case A64Opcode::SCVTF:
    case A64Opcode::FCVTZS:
    case A64Opcode::SMULH:
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

auto IsRematerializable(A64Opcode op) -> bool {
  return op == A64Opcode::MOVZ || op == A64Opcode::MOVN || op == A64Opcode::ADRP;
}

auto IsTransfer(A64Opcode op) -> bool {
  return op == A64Opcode::B || op == A64Opcode::B_COND || op == A64Opcode::CBZ || op == A64Opcode::CBNZ ||
         op == A64Opcode::RET;
}

struct Loop {
  TargetBlock *header = nullptr;
  TargetBlock *preheader = nullptr;
  std::unordered_set<TargetBlock *> body;
};

class Impl {
 public:
  explicit Impl(TargetFunction *func) : func_(func) {}

  auto Run() -> bool {
    if (std::getenv("AARCH64_MACHINE_LICM_OFF") != nullptr || func_->GetBlocks().size() < 2) {
      return false;
    }
    BuildCFG();
    BuildDefCounts();
    ComputeDominators();
    auto loops = FindLoops();
    int function_gpr_peak = 0;
    int function_fpr_peak = 0;
    for (const auto &loop : loops) {
      function_gpr_peak = std::max(function_gpr_peak, PeakPressure(loop, TargetRegClass::General));
      function_fpr_peak = std::max(function_fpr_peak, PeakPressure(loop, TargetRegClass::Float));
    }
    // Allocation is function-wide.  A single very pressured loop is enough for
    // an otherwise local live-range extension to change coloring and introduce
    // spills elsewhere, especially after unrolling.  Leave such functions
    // untouched; their invariant arithmetic is cheaper than a spill/reload.
    if (function_gpr_peak >= 22 || function_fpr_peak >= 30) return false;
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
    std::unordered_map<std::string, TargetBlock *> labels;
    for (size_t i = 0; i < blocks.size(); ++i) {
      index_[blocks[i]] = i;
      labels[blocks[i]->GetLabel()] = blocks[i];
    }
    for (size_t i = 0; i < blocks.size(); ++i) {
      auto *block = blocks[i];
      auto &out = succ_[block];
      for (auto *inst : block->GetInsts()) {
        if (!IsTransfer(static_cast<AArch64Inst *>(inst)->GetOpcode())) {
          continue;
        }
        for (auto *operand : inst->GetOperands()) {
          if (operand->GetOpType() != TargetOperandType::Label) {
            continue;
          }
          auto found = labels.find(static_cast<TargetLabel *>(operand)->GetName());
          if (found != labels.end() && std::find(out.begin(), out.end(), found->second) == out.end()) {
            out.push_back(found->second);
          }
        }
      }
      bool falls_through = true;
      if (!block->GetInsts().empty()) {
        auto last = static_cast<AArch64Inst *>(block->GetInsts().back())->GetOpcode();
        falls_through = last != A64Opcode::B && last != A64Opcode::RET;
      }
      if (falls_through && i + 1 < blocks.size() && std::find(out.begin(), out.end(), blocks[i + 1]) == out.end()) {
        out.push_back(blocks[i + 1]);
      }
    }
    for (auto *block : blocks) {
      for (auto *next : succ_[block]) {
        pred_[next].push_back(block);
      }
    }
  }

  auto BuildDefCounts() -> void {
    for (auto *block : func_->GetBlocks()) {
      for (auto *inst : block->GetInsts()) {
        auto *result = inst->GetResult();
        if (result != nullptr && result->IsVirtualReg()) {
          ++def_count_[static_cast<TargetVirtualReg *>(result)->GetID()];
        }
      }
    }
  }

  auto ComputeDominators() -> void {
    const auto &blocks = func_->GetBlocks();
    std::unordered_set<size_t> all;
    for (size_t i = 0; i < blocks.size(); ++i) all.insert(i);
    dom_.resize(blocks.size());
    dom_[0] = {0};
    for (size_t i = 1; i < blocks.size(); ++i) dom_[i] = all;
    bool changed = true;
    while (changed) {
      changed = false;
      for (size_t i = 1; i < blocks.size(); ++i) {
        std::unordered_set<size_t> next;
        bool first = true;
        for (auto *parent : pred_[blocks[i]]) {
          if (first) {
            next = dom_[index_[parent]];
            first = false;
          } else {
            std::unordered_set<size_t> intersection;
            for (size_t item : next) {
              if (dom_[index_[parent]].count(item) != 0) intersection.insert(item);
            }
            next = std::move(intersection);
          }
        }
        next.insert(i);
        if (next != dom_[i]) {
          dom_[i] = std::move(next);
          changed = true;
        }
      }
    }
  }

  auto Dominates(TargetBlock *lhs, TargetBlock *rhs) const -> bool {
    return dom_[index_.at(rhs)].count(index_.at(lhs)) != 0;
  }

  auto FindPreheader(const Loop &loop) const -> TargetBlock * {
    TargetBlock *preheader = nullptr;
    auto found = pred_.find(loop.header);
    if (found == pred_.end()) return nullptr;
    for (auto *parent : found->second) {
      if (loop.body.count(parent) != 0) continue;
      if (preheader != nullptr) return nullptr;
      preheader = parent;
    }
    return preheader != nullptr && Dominates(preheader, loop.header) ? preheader : nullptr;
  }

  auto FindLoops() -> std::vector<Loop> {
    std::vector<Loop> loops;
    for (auto *tail : func_->GetBlocks()) {
      for (auto *header : succ_[tail]) {
        if (!Dominates(header, tail)) continue;
        Loop loop;
        loop.header = header;
        loop.body.insert(header);
        std::vector<TargetBlock *> work;
        if (tail != header) {
          loop.body.insert(tail);
          work.push_back(tail);
        }
        while (!work.empty()) {
          auto *block = work.back();
          work.pop_back();
          for (auto *parent : pred_[block]) {
            if (loop.body.insert(parent).second) work.push_back(parent);
          }
        }
        loop.preheader = FindPreheader(loop);
        loops.push_back(std::move(loop));
      }
    }
    return loops;
  }

  auto OperandInvariant(TargetOperand *operand, const std::unordered_set<size_t> &selected) const -> bool {
    switch (operand->GetOpType()) {
      case TargetOperandType::Immediate:
      case TargetOperandType::Symbol:
      case TargetOperandType::Label:
        return true;
      case TargetOperandType::VirtualReg: {
        size_t id = static_cast<TargetVirtualReg *>(operand)->GetID();
        return selected.count(id) != 0 || def_in_loop_.count(id) == 0;
      }
      default:
        return false;
    }
  }

  auto PeakPressure(const Loop &loop, TargetRegClass reg_class) const -> int {
    auto in_class = [reg_class](TargetOperand *operand) {
      return operand != nullptr && operand->IsVirtualReg() &&
             static_cast<TargetVirtualReg *>(operand)->GetRegClass() == reg_class;
    };
    std::unordered_map<TargetBlock *, std::unordered_set<size_t>> live_in;
    bool changed = true;
    int iterations = 0;
    while (changed && iterations++ < 64) {
      changed = false;
      for (auto *block : loop.body) {
        std::unordered_set<size_t> live;
        auto successors = succ_.find(block);
        if (successors != succ_.end()) {
          for (auto *next : successors->second) {
            if (loop.body.count(next) != 0) {
              live.insert(live_in[next].begin(), live_in[next].end());
            }
          }
        }
        const auto &insts = block->GetInsts();
        for (auto it = insts.rbegin(); it != insts.rend(); ++it) {
          auto *result = (*it)->GetResult();
          if (in_class(result)) live.erase(static_cast<TargetVirtualReg *>(result)->GetID());
          for (auto *operand : (*it)->GetOperands()) {
            if (in_class(operand)) live.insert(static_cast<TargetVirtualReg *>(operand)->GetID());
          }
        }
        if (live_in[block] != live) {
          live_in[block] = std::move(live);
          changed = true;
        }
      }
    }
    int peak = 0;
    for (auto *block : loop.body) {
      std::unordered_set<size_t> live;
      auto successors = succ_.find(block);
      if (successors != succ_.end()) {
        for (auto *next : successors->second) {
          if (loop.body.count(next) != 0) live.insert(live_in[next].begin(), live_in[next].end());
        }
      }
      peak = std::max(peak, static_cast<int>(live.size()));
      const auto &insts = block->GetInsts();
      for (auto it = insts.rbegin(); it != insts.rend(); ++it) {
        auto *result = (*it)->GetResult();
        if (in_class(result)) live.erase(static_cast<TargetVirtualReg *>(result)->GetID());
        for (auto *operand : (*it)->GetOperands()) {
          if (in_class(operand)) live.insert(static_cast<TargetVirtualReg *>(operand)->GetID());
        }
        peak = std::max(peak, static_cast<int>(live.size()));
      }
    }
    return peak;
  }

  auto HoistLoop(const Loop &loop) -> bool {
    int gpr_peak = PeakPressure(loop, TargetRegClass::General);
    int fpr_peak = PeakPressure(loop, TargetRegClass::Float);
    // Leave six GPRs and four FPRs of headroom for physical argument values,
    // allocator temporaries and values from enclosing loops, none of which this
    // local estimate models precisely.
    if (gpr_peak >= 20 || fpr_peak >= 28) return false;

    def_in_loop_.clear();
    for (auto *block : loop.body) {
      for (auto *inst : block->GetInsts()) {
        auto *result = inst->GetResult();
        if (result != nullptr && result->IsVirtualReg()) {
          def_in_loop_.insert(static_cast<TargetVirtualReg *>(result)->GetID());
        }
      }
    }

    std::vector<TargetBlock *> ordered;
    for (auto *block : func_->GetBlocks()) {
      if (loop.body.count(block) != 0) ordered.push_back(block);
    }
    std::unordered_set<size_t> selected;
    std::unordered_set<TargetInst *> moved;
    std::vector<std::pair<TargetBlock *, TargetInst *>> candidates;
    bool progress = true;
    while (progress) {
      progress = false;
      for (auto *block : ordered) {
        for (auto *inst : block->GetInsts()) {
          if (moved.count(inst) != 0) continue;
          auto opcode = static_cast<AArch64Inst *>(inst)->GetOpcode();
          if (!IsHoistable(opcode)) continue;
          auto *result = inst->GetResult();
          if (result == nullptr || !result->IsVirtualReg()) continue;
          size_t id = static_cast<TargetVirtualReg *>(result)->GetID();
          if (def_count_[id] != 1) continue;
          bool invariant = true;
          for (auto *operand : inst->GetOperands()) {
            if (!OperandInvariant(operand, selected)) {
              invariant = false;
              break;
            }
          }
          if (!invariant) continue;
          selected.insert(id);
          moved.insert(inst);
          candidates.emplace_back(block, inst);
          progress = true;
        }
      }
    }
    if (candidates.empty()) return false;

    // Constant/address materialization is profitable only as part of a larger
    // invariant chain.  Standalone hoisting merely lengthens its live range.
    std::unordered_set<size_t> consumed_by_candidate;
    for (const auto &[block, inst] : candidates) {
      (void)block;
      for (auto *operand : inst->GetOperands()) {
        if (operand->IsVirtualReg()) {
          consumed_by_candidate.insert(static_cast<TargetVirtualReg *>(operand)->GetID());
        }
      }
    }
    // AArch64 constants are cheap to rematerialize and the allocator already
    // does so when profitable.  Keeping one live through nested unrolled loops
    // can spill several hotter address values, so machine LICM does not hoist a
    // materialization itself; it only moves arithmetic fed by values that were
    // already available outside the loop.
    constexpr int kRematCap = 0;
    int remat_count = 0;
    std::unordered_set<size_t> rejected;
    for (const auto &[block, inst] : candidates) {
      (void)block;
      auto opcode = static_cast<AArch64Inst *>(inst)->GetOpcode();
      if (!IsRematerializable(opcode)) continue;
      size_t id = static_cast<TargetVirtualReg *>(inst->GetResult())->GetID();
      if (consumed_by_candidate.count(id) == 0 || remat_count >= kRematCap)
        rejected.insert(id);
      else
        ++remat_count;
    }
    // Reject every dependent instruction if one of its in-loop producers was
    // rejected.  Iterate because address chains can contain several levels.
    bool pruned = true;
    while (pruned) {
      pruned = false;
      for (const auto &[block, inst] : candidates) {
        (void)block;
        size_t id = static_cast<TargetVirtualReg *>(inst->GetResult())->GetID();
        if (rejected.count(id) != 0) continue;
        for (auto *operand : inst->GetOperands()) {
          if (operand->IsVirtualReg() && rejected.count(static_cast<TargetVirtualReg *>(operand)->GetID()) != 0) {
            rejected.insert(id);
            pruned = true;
            break;
          }
        }
      }
    }

    moved.clear();
    std::vector<TargetInst *> sequence;
    for (const auto &[block, inst] : candidates) {
      (void)block;
      size_t id = static_cast<TargetVirtualReg *>(inst->GetResult())->GetID();
      if (rejected.count(id) == 0) {
        moved.insert(inst);
        sequence.push_back(inst);
      }
    }
    if (sequence.empty()) return false;
    for (auto *block : ordered) {
      std::vector<TargetInst *> kept;
      for (auto *inst : block->GetInsts()) {
        if (moved.count(inst) == 0) kept.push_back(inst);
      }
      block->SetInsts(std::move(kept));
    }
    auto preheader = loop.preheader->GetInsts();
    size_t insert = preheader.size();
    while (insert > 0 && IsTransfer(static_cast<AArch64Inst *>(preheader[insert - 1])->GetOpcode())) --insert;
    preheader.insert(preheader.begin() + static_cast<long>(insert), sequence.begin(), sequence.end());
    loop.preheader->SetInsts(std::move(preheader));
    return true;
  }

  TargetFunction *func_;
  std::unordered_map<TargetBlock *, size_t> index_;
  std::unordered_map<TargetBlock *, std::vector<TargetBlock *>> succ_;
  std::unordered_map<TargetBlock *, std::vector<TargetBlock *>> pred_;
  std::vector<std::unordered_set<size_t>> dom_;
  std::unordered_map<size_t, int> def_count_;
  std::unordered_set<size_t> def_in_loop_;
};

}  // namespace

auto AArch64MachineLICM::RunOnFunction(TargetFunction *func) -> bool { return Impl(func).Run(); }

}  // namespace backend::targetir::aarch64
