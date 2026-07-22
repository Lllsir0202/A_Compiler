#include "backend/targets/rv64/inst/cmp/cmp.h"
#include "backend/targets/utils/operand/targetoperand.h"

namespace backend::targetir::rv64 {
// ICmp
auto RVSLTInst::Emit(std::ofstream *out) -> void {
  *out << "    slt ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << '\n';
}

auto RVSLTInst::Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> RVSLTInst * {
  auto *inst = new RVSLTInst();
  inst->operands_.reserve(2);
  inst->AddOperand(rs1);
  inst->AddOperand(rs2);
  inst->SetResult(rd);
  return inst;
}

auto RVSLEInst::Emit(std::ofstream *out) -> void {
  *out << "    sle ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << '\n';
}

auto RVSLEInst::Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> RVSLEInst * {
  auto *inst = new RVSLEInst();
  inst->operands_.reserve(2);
  inst->AddOperand(rs1);
  inst->AddOperand(rs2);
  inst->SetResult(rd);
  return inst;
}

auto RVSLTIInst::Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *imm) -> RVSLTIInst * {
  auto *inst = new RVSLTIInst();
  inst->operands_.reserve(2);
  inst->AddOperand(rs1);
  inst->AddOperand(imm);
  inst->SetResult(rd);
  return inst;
}

auto RVSLTIInst::Emit(std::ofstream *out) -> void {
  *out << "    slti ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << '\n';
}

// FCmp
auto RVFLTSInst::Emit(std::ofstream *out) -> void {
  *out << "    flt.s ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << '\n';
}

auto RVFLTSInst::Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> RVFLTSInst * {
  auto *inst = new RVFLTSInst();
  inst->operands_.reserve(2);
  inst->AddOperand(rs1);
  inst->AddOperand(rs2);
  inst->SetResult(rd);
  return inst;
}

auto RVFLESInst::Emit(std::ofstream *out) -> void {
  *out << "    fle.s ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << '\n';
}

auto RVFLESInst::Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> RVFLESInst * {
  auto *inst = new RVFLESInst();
  inst->operands_.reserve(2);
  inst->AddOperand(rs1);
  inst->AddOperand(rs2);
  inst->SetResult(rd);
  return inst;
}

auto RVFEQSInst::Emit(std::ofstream *out) -> void {
  *out << "    feq.s ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << '\n';
}

auto RVFEQSInst::Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> RVFEQSInst * {
  auto *inst = new RVFEQSInst();
  inst->operands_.reserve(2);
  inst->AddOperand(rs1);
  inst->AddOperand(rs2);
  inst->SetResult(rd);
  return inst;
}

}  // namespace backend::targetir::rv64
