#include "backend/targets/riscv/pass/twoaddress.h"

#include <unordered_map>
#include <vector>

#include "backend/targets/riscv/inst/riscvinst.h"
#include "backend/targets/utils/operand/targetreg.h"
#include "backend/targets/utils/struct/targetblock.h"
#include "backend/targets/utils/struct/targetfunction.h"

namespace backend::targetir::riscv {

namespace {

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
  return changed;
}

}  // namespace backend::targetir::riscv
