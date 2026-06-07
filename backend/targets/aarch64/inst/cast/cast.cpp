#include "backend/targets/aarch64/inst/cast/cast.h"
#include "backend/targets/utils/operand/targetoperand.h"

namespace backend::targetir::aarch64 {
auto ARMSCVTFInst::Emit(std::ofstream *out) -> void {
  *out << "    scvtf ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << '\n';
}

auto ARMSCVTFInst::Create(TargetOperand *rd, TargetOperand *rs1) -> ARMSCVTFInst * {
  auto *inst = new ARMSCVTFInst();
  inst->operands_.reserve(1);
  inst->AddOperand(rs1);
  inst->SetResult(rd);
  return inst;
}

auto ARMUCVTFInst::Emit(std::ofstream *out) -> void {
  *out << "    ucvtf ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << '\n';
}

auto ARMUCVTFInst::Create(TargetOperand *rd, TargetOperand *rs1) -> ARMUCVTFInst * {
  auto *inst = new ARMUCVTFInst();
  inst->operands_.reserve(1);
  inst->AddOperand(rs1);
  inst->SetResult(rd);
  return inst;
}

auto ARMFCVTZSInst::Emit(std::ofstream *out) -> void {
  *out << "    fcvtzs ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << '\n';
}

auto ARMFCVTZSInst::Create(TargetOperand *rd, TargetOperand *rs1) -> ARMFCVTZSInst * {
  auto *inst = new ARMFCVTZSInst();
  inst->operands_.reserve(1);
  inst->AddOperand(rs1);
  inst->SetResult(rd);
  return inst;
}

}  // namespace backend::targetir::aarch64