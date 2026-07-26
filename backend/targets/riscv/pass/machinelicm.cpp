#include "backend/targets/riscv/pass/machinelicm.h"

#include <algorithm>
#include <unordered_set>
#include <utility>
#include <vector>
#include "backend/targets/riscv/inst/riscvinst.h"
#include "backend/targets/riscv/pass/machineanalysis.h"
#include "backend/targets/riscv/regs/riscvreg.h"
#include "backend/targets/utils/operand/targetreg.h"
#include "backend/targets/utils/struct/targetblock.h"
#include "backend/targets/utils/struct/targetfunction.h"

namespace backend::targetir::riscv {

namespace {

auto IsX0(TargetOperand *operand) -> bool {
  if (operand == nullptr || !operand->IsPhysicalReg()) {
    return false;
  }
  auto *reg = static_cast<TargetPhysicalReg *>(operand);
  return reg->GetRegClass() == TargetRegClass::General && reg->GetID() == static_cast<uint8_t>(GPR::Zero);
}

auto IsCheapSpeculatable(RVOpcode opcode) -> bool {
  const auto &info = GetRVOpcodeInfo(opcode);
  // Integer division is architecturally non-trapping on RISC-V, but moving it
  // onto a zero-trip path is expensive. Loads may trap and require aliasing.
  return info.IsPure() && opcode != RVOpcode::DIVW && opcode != RVOpcode::REMW && opcode != RVOpcode::FDIV_S;
}

auto InsertBeforeTerminators(TargetBlock *block, const std::vector<TargetInst *> &instructions) -> void {
  std::vector<TargetInst *> rewritten = block->GetInsts();
  size_t insertion = rewritten.size();
  while (insertion > 0) {
    auto opcode = static_cast<RISCVInst *>(rewritten[insertion - 1])->GetOpcode();
    if (!GetRVOpcodeInfo(opcode).HasFlag(RVInstFlag::Terminator)) {
      break;
    }
    --insertion;
  }
  rewritten.insert(rewritten.begin() + static_cast<long>(insertion), instructions.begin(), instructions.end());
  block->SetInsts(std::move(rewritten));
}

class MachineLICMImpl {
 public:
  explicit MachineLICMImpl(TargetFunction *function) : function_(function), cfg_(function) {}

  auto Run() -> bool {
    bool changed = false;
    for (const auto &loop : cfg_.NaturalLoops()) {
      if (loop.preheader_ != nullptr) {
        changed |= Hoist(loop);
      }
    }
    return changed;
  }

 private:
  auto OperandInvariant(TargetOperand *operand, const MachineNaturalLoop &loop,
                        const std::unordered_set<TargetOperand *> &hoisted) const -> bool {
    if (operand == nullptr) {
      return false;
    }
    switch (operand->GetOpType()) {
      case TargetOperandType::Immediate:
      case TargetOperandType::Symbol:
      case TargetOperandType::Label:
        return true;
      case TargetOperandType::PhysicalReg:
        return IsX0(operand);
      case TargetOperandType::VirtualReg: {
        if (hoisted.count(operand) != 0) {
          return true;
        }
        auto iter = definition_blocks_.find(operand);
        if (iter == definition_blocks_.end()) {
          return false;
        }
        for (auto *definition_block : iter->second) {
          if (loop.blocks_.count(definition_block) != 0) {
            return false;
          }
        }
        return true;
      }
      default:
        return false;
    }
  }

  auto BuildDefinitions() -> void {
    definition_count_.clear();
    definition_blocks_.clear();
    for (auto *block : function_->GetBlocks()) {
      for (auto *instruction : block->GetInsts()) {
        auto *result = instruction->GetResult();
        if (result != nullptr && result->IsVirtualReg()) {
          definition_count_[result]++;
          definition_blocks_[result].insert(block);
        }
      }
    }
  }

  auto Hoist(const MachineNaturalLoop &loop) -> bool {
    BuildDefinitions();

    constexpr int kRematerializableCap = 3;
    // Keep a small absolute budget even when full edge-aware liveness reports
    // high exit pressure. Repeated magic-division constants are otherwise
    // rematerialized on every iteration; the cap prevents unbounded pressure.
    int gpr_budget = kRematerializableCap;
    int fpr_budget = kRematerializableCap;

    // Charge constants already hoisted by an earlier pass-manager iteration.
    for (auto *instruction : loop.preheader_->GetInsts()) {
      auto opcode = static_cast<RISCVInst *>(instruction)->GetOpcode();
      if (!GetRVOpcodeInfo(opcode).HasFlag(RVInstFlag::Rematerializable)) {
        continue;
      }
      auto *result = instruction->GetResult();
      bool used_in_loop = false;
      for (auto *block : loop.blocks_) {
        for (auto *candidate : block->GetInsts()) {
          for (auto *operand : candidate->GetOperands()) {
            used_in_loop |= operand == result;
          }
        }
      }
      if (used_in_loop && result != nullptr && result->IsVirtualReg()) {
        auto reg_class = static_cast<TargetVirtualReg *>(result)->GetRegClass();
        int &budget = reg_class == TargetRegClass::Float ? fpr_budget : gpr_budget;
        budget = std::max(0, budget - 1);
      }
    }

    std::vector<TargetBlock *> ordered_blocks;
    for (auto *block : function_->GetBlocks()) {
      if (loop.blocks_.count(block) != 0) {
        ordered_blocks.push_back(block);
      }
    }

    std::unordered_set<TargetOperand *> hoisted_values;
    std::unordered_set<TargetInst *> moved;
    std::vector<std::pair<TargetBlock *, TargetInst *>> move_order;
    bool progress = true;
    while (progress) {
      progress = false;
      for (auto *block : ordered_blocks) {
        for (auto *instruction : block->GetInsts()) {
          if (moved.count(instruction) != 0) {
            continue;
          }
          auto opcode = static_cast<RISCVInst *>(instruction)->GetOpcode();
          auto *result = instruction->GetResult();
          if (!IsCheapSpeculatable(opcode) || result == nullptr || !result->IsVirtualReg() ||
              definition_count_[result] != 1) {
            continue;
          }
          bool invariant = true;
          for (auto *operand : instruction->GetOperands()) {
            invariant &= OperandInvariant(operand, loop, hoisted_values);
          }
          if (!invariant) {
            continue;
          }
          if (GetRVOpcodeInfo(opcode).HasFlag(RVInstFlag::Rematerializable)) {
            auto reg_class = static_cast<TargetVirtualReg *>(result)->GetRegClass();
            int &budget = reg_class == TargetRegClass::Float ? fpr_budget : gpr_budget;
            if (budget == 0) {
              continue;
            }
            --budget;
          }
          hoisted_values.insert(result);
          moved.insert(instruction);
          move_order.emplace_back(block, instruction);
          progress = true;
        }
      }
    }
    if (move_order.empty()) {
      return false;
    }

    for (auto *block : ordered_blocks) {
      std::vector<TargetInst *> kept;
      for (auto *instruction : block->GetInsts()) {
        if (moved.count(instruction) == 0) {
          kept.push_back(instruction);
        }
      }
      block->SetInsts(std::move(kept));
    }
    std::vector<TargetInst *> sequence;
    for (const auto &[block, instruction] : move_order) {
      (void)block;
      sequence.push_back(instruction);
    }
    InsertBeforeTerminators(loop.preheader_, sequence);
    return true;
  }

  TargetFunction *function_;
  MachineCFG cfg_;
  std::unordered_map<TargetOperand *, size_t> definition_count_;
  std::unordered_map<TargetOperand *, std::unordered_set<TargetBlock *>> definition_blocks_;
};

}  // namespace

auto MachineLICM::RunOnFunction(TargetFunction *function) -> bool {
  MachineLICMImpl implementation(function);
  return implementation.Run();
}

}  // namespace backend::targetir::riscv
