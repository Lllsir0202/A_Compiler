#include "backend/targets/riscv/pass/machineverifier.h"

#include <cstddef>
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "backend/targets/riscv/inst/riscvinst.h"
#include "backend/targets/utils/operand/targetliteral.h"
#include "backend/targets/utils/operand/targetoperand.h"
#include "backend/targets/utils/operand/targetreg.h"
#include "backend/targets/utils/operand/targetsymbol.h"
#include "backend/targets/utils/struct/targetblock.h"
#include "backend/targets/utils/struct/targetfunction.h"
#include "context/compiler_context.h"

namespace backend::targetir::riscv {

namespace {

auto IsRegister(TargetOperand *operand) -> bool {
  return operand != nullptr && (operand->IsVirtualReg() || operand->IsPhysicalReg());
}

auto IsSigned12(int value) -> bool { return value >= -2048 && value <= 2047; }

auto VerifyImmediate(RVOpcode opcode, TargetOperand *operand) -> bool {
  if (operand == nullptr || operand->GetOpType() != TargetOperandType::Immediate) {
    return false;
  }
  int value = static_cast<TargetIntLiteral *>(operand)->GetValue();
  switch (opcode) {
    case RVOpcode::SLLIW:
    case RVOpcode::SRLIW:
    case RVOpcode::SRAIW:
      return value >= 0 && value <= 31;
    case RVOpcode::SLLI:
    case RVOpcode::SRAI:
      return value >= 0 && value <= 63;
    default:
      return IsSigned12(value);
  }
}

}  // namespace

auto RISCVVerifier::Error(const std::string &message) -> bool {
  std::cerr << "RISC-V machine verifier: " << message << '\n';
  compiler_context_->ReportError("RISC-V machine verifier: " + message);
  return false;
}

auto RISCVVerifier::VerifyInst(TargetInst *inst, MachineVerifyStage stage, const std::string &location) -> bool {
  if (inst == nullptr) {
    return Error(location + ": null instruction");
  }
  auto *riscv_inst = static_cast<RISCVInst *>(inst);
  auto opcode = riscv_inst->GetOpcode();
  const auto &info = GetRVOpcodeInfo(opcode);
  const auto &operands = inst->GetOperands();
  auto *result = inst->GetResult();

  if (info.num_operands_ >= 0 && operands.size() != static_cast<size_t>(info.num_operands_)) {
    return Error(location + ": " + info.mnemonic_ + " expects " + std::to_string(info.num_operands_) +
                 " operands, got " + std::to_string(operands.size()));
  }
  if (info.num_operands_ < 0 && operands.empty()) {
    return Error(location + ": variadic " + info.mnemonic_ + " has no callee operand");
  }
  if (info.has_result_ != (result != nullptr)) {
    return Error(location + ": " + info.mnemonic_ +
                 (info.has_result_ ? " requires a result" : " must not have a result"));
  }
  if (result != nullptr && !IsRegister(result)) {
    return Error(location + ": " + info.mnemonic_ + " result is not a register");
  }
  for (auto *operand : operands) {
    if (operand == nullptr) {
      return Error(location + ": " + info.mnemonic_ + " has a null operand");
    }
  }

  if (stage == MachineVerifyStage::PostRegisterAllocation) {
    if (result != nullptr && result->IsVirtualReg()) {
      return Error(location + ": virtual result remains after register allocation");
    }
    for (auto *operand : operands) {
      if (operand->IsVirtualReg()) {
        return Error(location + ": virtual operand remains after register allocation");
      }
    }
  }

  switch (info.form_) {
    case RVForm::RType:
      if (!IsRegister(operands[0]) || !IsRegister(operands[1])) {
        return Error(location + ": " + info.mnemonic_ + " requires two register operands");
      }
      break;
    case RVForm::IType:
      if (!IsRegister(operands[0]) || !VerifyImmediate(opcode, operands[1])) {
        return Error(location + ": " + info.mnemonic_ + " has an illegal register/immediate operand");
      }
      break;
    case RVForm::Unary:
    case RVForm::ConvertToInt:
      if (!IsRegister(operands[0])) {
        return Error(location + ": " + info.mnemonic_ + " requires a register operand");
      }
      break;
    case RVForm::Load:
      if (!IsRegister(operands[0]) || !VerifyImmediate(RVOpcode::ADDI, operands[1])) {
        return Error(location + ": " + info.mnemonic_ + " has an illegal address");
      }
      break;
    case RVForm::Store:
      if (!IsRegister(operands[0]) || !IsRegister(operands[1]) || !VerifyImmediate(RVOpcode::ADDI, operands[2])) {
        return Error(location + ": " + info.mnemonic_ + " has an illegal value/address");
      }
      break;
    case RVForm::Branch:
      if (!IsRegister(operands[0]) || !IsRegister(operands[1]) ||
          operands[2]->GetOpType() != TargetOperandType::Label) {
        return Error(location + ": " + info.mnemonic_ + " has illegal branch operands");
      }
      break;
    case RVForm::Jump:
      if (operands[0]->GetOpType() != TargetOperandType::Label) {
        return Error(location + ": jump target is not a block label");
      }
      break;
    case RVForm::LoadImm:
      if (operands[0]->GetOpType() != TargetOperandType::Immediate) {
        return Error(location + ": li operand is not an immediate");
      }
      break;
    case RVForm::LoadSym:
      if (operands[0]->GetOpType() != TargetOperandType::Symbol) {
        return Error(location + ": la operand is not a symbol");
      }
      break;
    case RVForm::Call:
      if (operands[0]->GetOpType() != TargetOperandType::Symbol) {
        return Error(location + ": call target is not a symbol");
      }
      for (size_t i = 1; i < operands.size(); ++i) {
        if (!IsRegister(operands[i])) {
          return Error(location + ": call argument-use operand is not a register");
        }
      }
      break;
    case RVForm::Ret:
      break;
    case RVForm::Phi:
      if (stage != MachineVerifyStage::PreRegisterAllocation) {
        return Error(location + ": machine phi remains after phi elimination");
      }
      if (operands.size() < 2 || operands.size() % 2 != 0) {
        return Error(location + ": phi requires value/block operand pairs");
      }
      for (size_t i = 0; i < operands.size(); i += 2) {
        if (!IsRegister(operands[i]) || operands[i + 1]->GetOpType() != TargetOperandType::Label) {
          return Error(location + ": phi has an illegal incoming value/block pair");
        }
      }
      break;
  }
  return true;
}

auto RISCVVerifier::Verify(TargetFunction *func, MachineVerifyStage stage) -> bool {
  if (func == nullptr) {
    return Error("null function");
  }
  const auto &blocks = func->GetBlocks();
  if (blocks.empty()) {
    return Error("function " + func->GetName() + " has no blocks");
  }

  std::unordered_set<std::string> labels;
  for (auto *block : blocks) {
    if (block == nullptr) {
      return Error("function " + func->GetName() + " contains a null block");
    }
    if (block->GetLabel().empty() || !labels.insert(block->GetLabel()).second) {
      return Error("function " + func->GetName() + " has an empty or duplicate block label");
    }
  }

  std::unordered_map<TargetOperand *, size_t> virtual_def_count;
  std::unordered_map<TargetOperand *, std::vector<std::string>> virtual_def_locations;
  std::unordered_map<std::string, std::unordered_set<std::string>> predecessors;
  for (auto *block : blocks) {
    const auto &insts = block->GetInsts();
    for (size_t index = 0; index < insts.size(); ++index) {
      auto *inst = insts[index];
      auto *result = inst->GetResult();
      if (result != nullptr && result->IsVirtualReg()) {
        virtual_def_count[result]++;
        virtual_def_locations[result].push_back(block->GetLabel() + ":" + std::to_string(index) + "(" +
                                                GetRVOpcodeInfo(static_cast<RISCVInst *>(inst)->GetOpcode()).mnemonic_ +
                                                ")");
      }
      const auto &info = GetRVOpcodeInfo(static_cast<RISCVInst *>(inst)->GetOpcode());
      if (!info.HasFlag(RVInstFlag::Branch)) {
        continue;
      }
      for (auto *operand : inst->GetOperands()) {
        if (operand->GetOpType() == TargetOperandType::Label) {
          predecessors[static_cast<TargetLabel *>(operand)->GetName()].insert(block->GetLabel());
        }
      }
    }
  }
  if (stage == MachineVerifyStage::PreRegisterAllocation) {
    for (const auto &[reg, count] : virtual_def_count) {
      if (count != 1) {
        auto *vreg = static_cast<TargetVirtualReg *>(reg);
        std::string locations;
        for (const auto &location : virtual_def_locations[reg]) {
          locations += (locations.empty() ? "" : ", ") + location;
        }
        return Error("function " + func->GetName() + " violates machine SSA: v" + std::to_string(vreg->GetID()) +
                     " has " + std::to_string(count) + " definitions at " + locations);
      }
    }
  }

  for (auto *block : blocks) {
    bool saw_terminator = false;
    bool saw_non_phi = false;
    const auto &insts = block->GetInsts();
    for (size_t index = 0; index < insts.size(); ++index) {
      auto location = func->GetName() + ":" + block->GetLabel() + ":" + std::to_string(index);
      if (!VerifyInst(insts[index], stage, location)) {
        return false;
      }
      auto *riscv_inst = static_cast<RISCVInst *>(insts[index]);
      const auto &info = GetRVOpcodeInfo(riscv_inst->GetOpcode());
      if (riscv_inst->GetOpcode() == RVOpcode::PHI) {
        if (saw_non_phi) {
          return Error(location + ": phi follows a non-phi instruction");
        }
        const auto &operands = insts[index]->GetOperands();
        for (size_t incoming = 1; incoming < operands.size(); incoming += 2) {
          const auto &pred_label = static_cast<TargetLabel *>(operands[incoming])->GetName();
          if (labels.count(pred_label) == 0 || predecessors[block->GetLabel()].count(pred_label) == 0) {
            return Error(location + ": phi incoming block is not a CFG predecessor");
          }
        }
      } else {
        saw_non_phi = true;
      }
      if (saw_terminator && !info.HasFlag(RVInstFlag::Terminator)) {
        return Error(location + ": non-terminator follows a terminator");
      }
      saw_terminator |= info.HasFlag(RVInstFlag::Terminator);

      if (info.HasFlag(RVInstFlag::Branch)) {
        for (auto *operand : insts[index]->GetOperands()) {
          if (operand->GetOpType() == TargetOperandType::Label &&
              labels.count(static_cast<TargetLabel *>(operand)->GetName()) == 0) {
            return Error(location + ": branch references an unknown block label");
          }
        }
      }
    }
  }
  return true;
}

}  // namespace backend::targetir::riscv
