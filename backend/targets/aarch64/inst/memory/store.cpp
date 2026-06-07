#include "backend/targets/aarch64/inst/memory/store.h"
#include "backend/targets/utils/operand/targetoperand.h"

namespace backend::targetir::aarch64 {
auto ARMSTRWInst::Emit(std::ofstream *out) -> void {
  *out << "    str ";
  operands_[0]->Emit(out);
  *out << ", [";
  operands_[1]->Emit(out);
  if (operands_[2] != nullptr) {
    *out << ", ";
    operands_[2]->Emit(out);
  }
  *out << "]\n";
}

auto ARMSTRWInst::Create(TargetOperand *rs2, TargetOperand *rs1, TargetOperand *rs2_offset) -> ARMSTRWInst * {
  auto *inst = new ARMSTRWInst();
  inst->operands_.push_back(rs2);
  inst->operands_.push_back(rs1);
  inst->operands_.push_back(rs2_offset);
  return inst;
}

auto ARMSTRWimmInst::Emit(std::ofstream *out) -> void {
  *out << "    str ";
  operands_[0]->Emit(out);
  *out << ", [";
  operands_[1]->Emit(out);
  *out << ", #";
  operands_[2]->Emit(out);
  *out << "]\n";
}

auto ARMSTRWimmInst::Create(TargetOperand *rs2, TargetOperand *rs1, TargetOperand *imm) -> ARMSTRWimmInst * {
  auto *inst = new ARMSTRWimmInst();
  inst->operands_.push_back(rs2);
  inst->operands_.push_back(rs1);
  inst->operands_.push_back(imm);
  return inst;
}

}  // namespace backend::targetir::aarch64