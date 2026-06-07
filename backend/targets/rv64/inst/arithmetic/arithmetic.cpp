#include "backend/targets/rv64/inst/arithmetic/arithmetic.h"
#include <cfloat>
#include "backend/targets/utils/operand/targetoperand.h"

namespace backend::targetir::rv64 {
auto RVADDInst::Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> RVADDInst * {
  auto *inst = new RVADDInst();
  inst->operands_.reserve(2);
  inst->AddOperand(rs1);
  inst->AddOperand(rs2);
  inst->SetResult(rd);
  return inst;
}

auto RVADDInst::Emit(std::ofstream *out) -> void {
  *out << "    add ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << '\n';
}

auto RVSUBInst::Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> RVSUBInst * {
  auto *inst = new RVSUBInst();
  inst->operands_.reserve(2);
  inst->AddOperand(rs1);
  inst->AddOperand(rs2);
  inst->SetResult(rd);
  return inst;
}

auto RVSUBInst::Emit(std::ofstream *out) -> void {
  *out << "    sub ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << '\n';
}

auto RVMULInst::Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> RVMULInst * {
  auto *inst = new RVMULInst();
  inst->operands_.reserve(2);
  inst->AddOperand(rs1);
  inst->AddOperand(rs2);
  inst->SetResult(rd);
  return inst;
}

auto RVMULInst::Emit(std::ofstream *out) -> void {
  *out << "    mul ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << '\n';
}

auto RVDIVInst::Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> RVDIVInst * {
  auto *inst = new RVDIVInst();
  inst->operands_.reserve(2);
  inst->AddOperand(rs1);
  inst->AddOperand(rs2);
  inst->SetResult(rd);
  return inst;
}

auto RVDIVInst::Emit(std::ofstream *out) -> void {
  *out << "    div ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << '\n';
}

auto RVREMInst::Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> RVREMInst * {
  auto *inst = new RVREMInst();
  inst->operands_.reserve(2);
  inst->AddOperand(rs1);
  inst->AddOperand(rs2);
  inst->SetResult(rd);
  return inst;
}

auto RVREMInst::Emit(std::ofstream *out) -> void {
  *out << "    rem ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << '\n';
}

auto RVADDIInst::Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *imm) -> RVADDIInst * {
  auto *inst = new RVADDIInst();
  inst->operands_.reserve(2);
  inst->AddOperand(rs1);
  inst->AddOperand(imm);
  inst->SetResult(rd);
  return inst;
}

auto RVADDIInst::Emit(std::ofstream *out) -> void {
  *out << "    addi ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << '\n';
}

auto RVADDWInst::Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> RVADDWInst * {
  auto *inst = new RVADDWInst();
  inst->operands_.reserve(2);
  inst->AddOperand(rs1);
  inst->AddOperand(rs2);
  inst->SetResult(rd);
  return inst;
}

auto RVADDWInst::Emit(std::ofstream *out) -> void {
  *out << "    addw ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << '\n';
}

// Float arithmetic instructions
auto RVFADDSInst::Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> RVFADDSInst * {
  auto *inst = new RVFADDSInst();
  inst->operands_.reserve(2);
  inst->AddOperand(rs1);
  inst->AddOperand(rs2);
  inst->SetResult(rd);
  return inst;
}

auto RVFADDSInst::Emit(std::ofstream *out) -> void {
  *out << "    fadds ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << '\n';
}

auto RVFSUBSInst::Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> RVFSUBSInst * {
  auto *inst = new RVFSUBSInst();
  inst->operands_.reserve(2);
  inst->AddOperand(rs1);
  inst->AddOperand(rs2);
  inst->SetResult(rd);
  return inst;
}

auto RVFSUBSInst::Emit(std::ofstream *out) -> void {
  *out << "    fsubs ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << '\n';
}

auto RVFMULSInst::Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> RVFMULSInst * {
  auto *inst = new RVFMULSInst();
  inst->operands_.reserve(2);
  inst->AddOperand(rs1);
  inst->AddOperand(rs2);
  inst->SetResult(rd);
  return inst;
}

auto RVFMULSInst::Emit(std::ofstream *out) -> void {
  *out << "    fmuls ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << '\n';
}

auto RVFDIVSInst::Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> RVFDIVSInst * {
  auto *inst = new RVFDIVSInst();
  inst->operands_.reserve(2);
  inst->AddOperand(rs1);
  inst->AddOperand(rs2);
  inst->SetResult(rd);
  return inst;
}

auto RVFDIVSInst::Emit(std::ofstream *out) -> void {
  *out << "    fdivs ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << '\n';
}

}  // namespace backend::targetir::rv64