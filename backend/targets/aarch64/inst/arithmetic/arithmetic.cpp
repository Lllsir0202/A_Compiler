#include "backend/targets/aarch64/inst/arithmetic/arithmetic.h"
#include "backend/targets/utils/operand/targetoperand.h"

namespace backend::targetir::aarch64 {
auto ARMADDInst::Emit(std::ofstream *out) -> void {
  *out << "    add ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << '\n';
}

auto ARMADDInst::Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> ARMADDInst * {
  auto inst = new ARMADDInst();
  inst->operands_.reserve(2);
  inst->AddOperand(rs1);
  inst->AddOperand(rs2);
  inst->SetResult(rd);
  return inst;
}

auto ARMSUBInst::Emit(std::ofstream *out) -> void {
  *out << "    sub ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << '\n';
}

auto ARMSUBInst::Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> ARMSUBInst * {
  auto inst = new ARMSUBInst();
  inst->operands_.reserve(2);
  inst->AddOperand(rs1);
  inst->AddOperand(rs2);
  inst->SetResult(rd);
  return inst;
}

auto ARMMULInst::Emit(std::ofstream *out) -> void {
  *out << "    mul ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << '\n';
}

auto ARMMULInst::Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> ARMMULInst * {
  auto inst = new ARMMULInst();
  inst->operands_.reserve(2);
  inst->AddOperand(rs1);
  inst->AddOperand(rs2);
  inst->SetResult(rd);
  return inst;
}

auto ARMDIVInst::Emit(std::ofstream *out) -> void {
  *out << "    div ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << '\n';
}

auto ARMDIVInst::Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> ARMDIVInst * {
  auto inst = new ARMDIVInst();
  inst->operands_.reserve(2);
  inst->AddOperand(rs1);
  inst->AddOperand(rs2);
  inst->SetResult(rd);
  return inst;
}

auto ARMADDImmInst::Emit(std::ofstream *out) -> void {
  *out << "    add ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << ", #";
  operands_[1]->Emit(out);
  *out << '\n';
}

auto ARMADDImmInst::Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *imm) -> ARMADDImmInst * {
  auto inst = new ARMADDImmInst();
  inst->operands_.reserve(2);
  inst->AddOperand(rs1);
  inst->AddOperand(imm);
  inst->SetResult(rd);
  return inst;
}

auto ARMSUBImmInst::Emit(std::ofstream *out) -> void {
  *out << "    sub ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << ", #";
  operands_[1]->Emit(out);
  *out << '\n';
}

auto ARMSUBImmInst::Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *imm) -> ARMSUBImmInst * {
  auto inst = new ARMSUBImmInst();
  inst->operands_.reserve(2);
  inst->AddOperand(rs1);
  inst->AddOperand(imm);
  inst->SetResult(rd);
  return inst;
}

auto ARMADDShiftInst::Emit(std::ofstream *out) -> void {
  *out << "    add ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << ", ";
  *out << ShiftTypeToString(type);
  *out << " #";
  operands_[2]->Emit(out);
  *out << '\n';
}

auto ARMADDShiftInst::Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2, TargetOperand *shift,
                             ShiftType type) -> ARMADDShiftInst * {
  auto inst = new ARMADDShiftInst();
  inst->operands_.reserve(3);
  inst->AddOperand(rs1);
  inst->AddOperand(rs2);
  inst->AddOperand(shift);
  inst->SetResult(rd);
  inst->type = type;
  return inst;
}

auto ARMSUBShiftInst::Emit(std::ofstream *out) -> void {
  *out << "    sub ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << ", ";
  *out << ShiftTypeToString(type);
  *out << " #";
  operands_[2]->Emit(out);
  *out << '\n';
}

auto ARMSUBShiftInst::Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2, TargetOperand *shift,
                             ShiftType type) -> ARMSUBShiftInst * {
  auto inst = new ARMSUBShiftInst();
  inst->operands_.reserve(3);
  inst->AddOperand(rs1);
  inst->AddOperand(rs2);
  inst->AddOperand(shift);
  inst->SetResult(rd);
  inst->type = type;
  return inst;
}

// Floating point arithmetic instructions
auto ARMFADDInst::Emit(std::ofstream *out) -> void {
  *out << "    fadd ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << '\n';
}

auto ARMFADDInst::Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> ARMFADDInst * {
  auto inst = new ARMFADDInst();
  inst->operands_.reserve(2);
  inst->AddOperand(rs1);
  inst->AddOperand(rs2);
  inst->SetResult(rd);
  return inst;
}

auto ARMFSUBInst::Emit(std::ofstream *out) -> void {
  *out << "    fsub ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << '\n';
}

auto ARMFSUBInst::Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> ARMFSUBInst * {
  auto inst = new ARMFSUBInst();
  inst->operands_.reserve(2);
  inst->AddOperand(rs1);
  inst->AddOperand(rs2);
  inst->SetResult(rd);
  return inst;
}

auto ARMFMULInst::Emit(std::ofstream *out) -> void {
  *out << "    fmul ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << '\n';
}

auto ARMFMULInst::Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> ARMFMULInst * {
  auto inst = new ARMFMULInst();
  inst->operands_.reserve(2);
  inst->AddOperand(rs1);
  inst->AddOperand(rs2);
  inst->SetResult(rd);
  return inst;
}

auto ARMFDIVInst::Emit(std::ofstream *out) -> void {
  *out << "    fdiv ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << '\n';
}

auto ARMFDIVInst::Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> ARMFDIVInst * {
  auto inst = new ARMFDIVInst();
  inst->operands_.reserve(2);
  inst->AddOperand(rs1);
  inst->AddOperand(rs2);
  inst->SetResult(rd);
  return inst;
}

}  // namespace backend::targetir::aarch64