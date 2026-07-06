#include "backend/targets/rv64/inst/cast/cast.h"
#include "backend/targets/utils/operand/targetoperand.h"

namespace backend::targetir::rv64 {
auto RVFCVTSWInst::Emit(std::ofstream *out) -> void {
  *out << "    fcvt.s.w ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << '\n';
}

auto RVFCVTSWInst::Create(TargetOperand *rd, TargetOperand *rs1) -> RVFCVTSWInst * {
  auto inst = new RVFCVTSWInst();
  inst->operands_.reserve(1);
  inst->AddOperand(rs1);
  inst->SetResult(rd);
  return inst;
}

auto RVFCVTWSInst::Emit(std::ofstream *out) -> void {
  *out << "    fcvt.w.s ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << '\n';
}

auto RVFCVTWSInst::Create(TargetOperand *rd, TargetOperand *rs1) -> RVFCVTWSInst * {
  auto inst = new RVFCVTWSInst();
  inst->operands_.reserve(1);
  inst->AddOperand(rs1);
  inst->SetResult(rd);
  return inst;
}

auto RVFCVTSWUInst::Emit(std::ofstream *out) -> void {
  *out << "    fcvt.s.wu ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << '\n';
}

auto RVFCVTSWUInst::Create(TargetOperand *rd, TargetOperand *rs1) -> RVFCVTSWUInst * {
  auto inst = new RVFCVTSWUInst();
  inst->operands_.reserve(1);
  inst->AddOperand(rs1);
  inst->SetResult(rd);
  return inst;
}

auto RVZextWInst::Emit(std::ofstream *out) -> void {
  *out << "    zext.w ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << '\n';
}

auto RVZextWInst::Create(TargetOperand *rd, TargetOperand *rs1) -> RVZextWInst * {
  auto inst = new RVZextWInst();
  inst->operands_.reserve(1);
  inst->AddOperand(rs1);
  inst->SetResult(rd);
  return inst;
}

}  // namespace backend::targetir::rv64
