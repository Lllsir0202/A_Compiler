#include "backend/targets/riscv/pass/addrfold.h"
#include "backend/targets/riscv/inst/riscvinst.h"
#include "backend/targets/utils/context/targetcontext.h"
#include "backend/targets/utils/operand/targetliteral.h"
#include "backend/targets/utils/operand/targetreg.h"
#include "backend/targets/utils/struct/targetblock.h"
#include "backend/targets/utils/struct/targetfunction.h"

namespace backend::targetir::riscv {
namespace {
constexpr int IMM_MIN = -2048;
constexpr int IMM_MAX = 2047;
}  // namespace

auto RISCVAddrFold::BuildDefs(TargetFunction *func) -> void {
  defs_.clear();
  def_count_.clear();
  for (auto *block : func->GetBlocks()) {
    for (auto *inst : block->GetInsts()) {
      auto *result = inst->GetResult();
      if (result != nullptr && result->IsVirtualReg()) {
        defs_[result] = inst;
        def_count_[result]++;
      }
    }
  }
}

auto RISCVAddrFold::SingleDef(TargetOperand *operand) const -> TargetInst * {
  if (operand == nullptr || !operand->IsVirtualReg()) {
    return nullptr;
  }
  auto count = def_count_.find(operand);
  if (count == def_count_.end() || count->second != 1) {
    return nullptr;
  }
  auto def = defs_.find(operand);
  return def == defs_.end() ? nullptr : def->second;
}

auto RISCVAddrFold::RunOnFunction(TargetFunction *func) -> bool {
  BuildDefs(func);
  bool changed = false;
  for (auto *block : func->GetBlocks()) {
    for (auto *inst : block->GetInsts()) {
      auto opcode = static_cast<RISCVInst *>(inst)->GetOpcode();
      bool is_load = opcode == RVOpcode::LW || opcode == RVOpcode::LD || opcode == RVOpcode::FLW;
      bool is_store = opcode == RVOpcode::SW || opcode == RVOpcode::SD || opcode == RVOpcode::FSW;
      if (!is_load && !is_store) {
        continue;
      }
      const auto &operands = inst->GetOperands();
      auto *base = is_load ? operands[0] : operands[1];
      auto *offset = is_load ? operands[1] : operands[2];
      if (offset->GetOpType() != TargetOperandType::Immediate) {
        continue;
      }
      int old_offset = static_cast<TargetIntLiteral *>(offset)->GetValue();
      auto *def = SingleDef(base);
      if (def == nullptr || static_cast<RISCVInst *>(def)->GetOpcode() != RVOpcode::ADDI) {
        continue;
      }
      const auto &def_operands = def->GetOperands();
      if (def_operands.size() != 2 || def_operands[1]->GetOpType() != TargetOperandType::Immediate) {
        continue;
      }
      int addend = static_cast<TargetIntLiteral *>(def_operands[1])->GetValue();
      int64_t combined = static_cast<int64_t>(old_offset) + addend;
      if (combined < IMM_MIN || combined > IMM_MAX) {
        continue;
      }
      auto *new_offset = ctx_->Imm(static_cast<int>(combined));
      if (is_load) {
        inst->SetOperands({def_operands[0], new_offset});
      } else {
        inst->SetOperands({operands[0], def_operands[0], new_offset});
      }
      changed = true;
    }
  }
  return changed;
}

}  // namespace backend::targetir::riscv
