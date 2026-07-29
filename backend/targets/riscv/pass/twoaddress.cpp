#include "backend/targets/riscv/pass/twoaddress.h"

#include <unordered_map>
#include <vector>

#include "backend/targets/riscv/inst/riscvinst.h"
#include "backend/targets/utils/operand/targetreg.h"
#include "backend/targets/utils/struct/targetblock.h"
#include "backend/targets/utils/struct/targetfunction.h"

namespace backend::targetir::riscv {

namespace {

struct InstLocation {
  TargetBlock *block_;
  size_t index_;
};

auto CompatibleVirtualRegisters(TargetOperand *lhs, TargetOperand *rhs) -> bool {
  if (lhs == nullptr || rhs == nullptr || !lhs->IsVirtualReg() || !rhs->IsVirtualReg()) {
    return false;
  }
  auto *left = static_cast<TargetVirtualReg *>(lhs);
  auto *right = static_cast<TargetVirtualReg *>(rhs);
  return left->GetRegClass() == right->GetRegClass() && left->GetRegSize() == right->GetRegSize();
}

auto IsCopyForClass(TargetInst *copy, TargetOperand *source) -> bool {
  auto opcode = static_cast<RISCVInst *>(copy)->GetOpcode();
  auto *reg = static_cast<TargetVirtualReg *>(source);
  return (opcode == RVOpcode::MV && reg->GetRegClass() == TargetRegClass::General) ||
         (opcode == RVOpcode::FMV_S && reg->GetRegClass() == TargetRegClass::Float);
}

auto TryPropagateLocalCopy(TargetFunction *func) -> bool {
  std::unordered_map<TargetOperand *, int> definitions;
  std::unordered_map<TargetOperand *, int> uses;
  std::unordered_map<TargetOperand *, InstLocation> definition_locations;
  std::unordered_map<TargetOperand *, std::vector<InstLocation>> use_locations;
  for (auto *block : func->GetBlocks()) {
    const auto &insts = block->GetInsts();
    for (size_t index = 0; index < insts.size(); ++index) {
      auto *inst = insts[index];
      auto *result = inst->GetResult();
      if (result != nullptr && result->IsVirtualReg()) {
        ++definitions[result];
        definition_locations[result] = {block, index};
      }
      for (auto *operand : inst->GetOperands()) {
        if (operand != nullptr && operand->IsVirtualReg()) {
          ++uses[operand];
          use_locations[operand].push_back({block, index});
        }
      }
    }
  }

  for (auto *block : func->GetBlocks()) {
    const auto &insts = block->GetInsts();
    for (size_t index = 0; index < insts.size(); ++index) {
      auto *copy = insts[index];
      const auto &copy_operands = copy->GetOperands();
      auto *destination = copy->GetResult();
      if (copy_operands.size() != 1) {
        continue;
      }
      auto *source = copy_operands[0];
      if (!CompatibleVirtualRegisters(source, destination) || !IsCopyForClass(copy, source) ||
          definitions[source] != 1 || uses[source] != 1 || definitions[destination] != 1) {
        continue;
      }
      auto def = definition_locations.find(source);
      if (def == definition_locations.end() || def->second.block_ != block || def->second.index_ >= index) {
        continue;
      }
      const auto &destination_uses = use_locations[destination];
      if (destination_uses.empty()) {
        continue;
      }
      bool local_and_after = true;
      for (const auto &use : destination_uses) {
        if (use.block_ != block || use.index_ <= index) {
          local_and_after = false;
          break;
        }
      }
      if (!local_and_after) {
        continue;
      }

      for (size_t use_index = index + 1; use_index < insts.size(); ++use_index) {
        insts[use_index]->ReplaceOperand(destination, source);
      }
      std::vector<TargetInst *> kept;
      kept.reserve(insts.size() - 1);
      for (size_t kept_index = 0; kept_index < insts.size(); ++kept_index) {
        if (kept_index != index) {
          kept.push_back(insts[kept_index]);
        }
      }
      block->SetInsts(std::move(kept));
      return true;
    }
  }
  return false;
}

}  // namespace

auto RISCVTwoAddress::RunOnFunction(TargetFunction *func) -> bool {
  std::unordered_map<TargetOperand *, int> definitions;
  std::unordered_map<TargetOperand *, int> uses;
  for (auto *block : func->GetBlocks()) {
    for (auto *inst : block->GetInsts()) {
      auto *result = inst->GetResult();
      if (result != nullptr && result->IsVirtualReg()) {
        ++definitions[result];
      }
      for (auto *operand : inst->GetOperands()) {
        if (operand != nullptr && operand->IsVirtualReg()) {
          ++uses[operand];
        }
      }
    }
  }

  bool changed = false;
  for (auto *block : func->GetBlocks()) {
    const auto &insts = block->GetInsts();
    std::vector<TargetInst *> kept;
    kept.reserve(insts.size());
    bool block_changed = false;
    for (size_t index = 0; index < insts.size(); ++index) {
      auto *producer = insts[index];
      if (index + 1 >= insts.size()) {
        kept.push_back(producer);
        continue;
      }
      auto *temporary = producer->GetResult();
      auto *copy = insts[index + 1];
      const auto &copy_operands = copy->GetOperands();
      auto *destination = copy->GetResult();
      // Phi lowering deliberately gives some virtual registers multiple
      // definitions.  Reusing one of those registers here would turn the
      // producer into another phi definition before the allocator has built
      // its interference graph, which is not a valid SSA copy coalescing
      // operation.  Keep both ends of this fold single-definition.
      bool foldable = CompatibleVirtualRegisters(temporary, destination) && copy_operands.size() == 1 &&
                      copy_operands[0] == temporary && definitions[temporary] == 1 && uses[temporary] == 1 &&
                      definitions[destination] == 1 && IsCopyForClass(copy, temporary);
      if (!foldable) {
        kept.push_back(producer);
        continue;
      }

      producer->ReplaceResult(destination);
      kept.push_back(producer);
      ++index;  // omit the now-redundant copy
      block_changed = true;
    }
    if (block_changed) {
      block->SetInsts(std::move(kept));
      changed = true;
    }
  }
  if (changed) {
    return true;
  }

  // Once adjacent result folding reaches a fixpoint, remove non-adjacent local
  // copies by propagating their single-use source through the destination's
  // later uses. Rebuild facts after each rewrite so copy chains stay safe.
  while (TryPropagateLocalCopy(func)) {
    changed = true;
  }
  return changed;
}

}  // namespace backend::targetir::riscv
