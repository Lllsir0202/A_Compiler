#include "backend/targets/aarch64/inst/memory/load.h"
#include "backend/targets/utils/operand/targetoperand.h"

namespace backend::targetir::aarch64 {
/**
 * @brief If the offset is nullptr, will be like "ldr rd, [rs1]"
 */
auto ARMLDRWInst::Emit(std::ofstream *out) -> void {
  *out << "    ldr ";
  result_->Emit(out);
  *out << ", [";
  operands_[0]->Emit(out);
  if (operands_[1] != nullptr) {
    *out << ", ";
    operands_[1]->Emit(out);
  }
  *out << "]\n";
}

auto ARMLDRWInst::Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> ARMLDRWInst * {
  auto *inst = new ARMLDRWInst();
  inst->result_ = rd;
  inst->operands_.push_back(rs1);
  inst->operands_.push_back(rs2);
  return inst;
}

auto ARMLDRWimmInst::Emit(std::ofstream *out) -> void {
  *out << "    ldr ";
  result_->Emit(out);
  *out << ", [";
  operands_[0]->Emit(out);
  *out << ", #";
  operands_[1]->Emit(out);
  *out << "]\n";
}

auto ARMLDRWimmInst::Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *imm) -> ARMLDRWimmInst * {
  auto *inst = new ARMLDRWimmInst();
  inst->result_ = rd;
  inst->operands_.push_back(rs1);
  inst->operands_.push_back(imm);
  return inst;
}

}  // namespace backend::targetir::aarch64