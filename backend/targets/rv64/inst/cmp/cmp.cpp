#include "backend/targets/rv64/inst/cmp/cmp.h"
#include "backend/targets/utils/operand/targetoperand.h"

namespace backend::targetir::rv64 {
// ICmp
auto RVSLTInst::Emit(std::ofstream *out) -> void {
  *out << "    slt ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << ", ";
  operands_[2]->Emit(out);
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
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << ", ";
  operands_[2]->Emit(out);
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

auto RVSEQInst::Emit(std::ofstream *out) -> void {
  *out << "    seq ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << ", ";
  operands_[2]->Emit(out);
  *out << '\n';
}

auto RVSEQInst::Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> RVSEQInst * {
  auto *inst = new RVSEQInst();
  inst->operands_.reserve(2);
  inst->AddOperand(rs1);
  inst->AddOperand(rs2);
  inst->SetResult(rd);
  return inst;
}

// FCmp
auto RVFLTInst::Emit(std::ofstream *out) -> void {
  *out << "    flt ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << ", ";
  operands_[2]->Emit(out);
  *out << '\n';
}

auto RVFLTInst::Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> RVFLTInst * {
  auto *inst = new RVFLTInst();
  inst->operands_.reserve(2);
  inst->AddOperand(rs1);
  inst->AddOperand(rs2);
  inst->SetResult(rd);
  return inst;
}

auto RVFLEInst::Emit(std::ofstream *out) -> void {
  *out << "    fle ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << ", ";
  operands_[2]->Emit(out);
  *out << '\n';
}

auto RVFLEInst::Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> RVFLEInst * {
  auto *inst = new RVFLEInst();
  inst->operands_.reserve(2);
  inst->AddOperand(rs1);
  inst->AddOperand(rs2);
  inst->SetResult(rd);
  return inst;
}

auto RVFEQInst::Emit(std::ofstream *out) -> void {
  *out << "    feq ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << ", ";
  operands_[2]->Emit(out);
  *out << '\n';
}

auto RVFEQInst::Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> RVFEQInst * {
  auto *inst = new RVFEQInst();
  inst->operands_.reserve(2);
  inst->AddOperand(rs1);
  inst->AddOperand(rs2);
  inst->SetResult(rd);
  return inst;
}

}  // namespace backend::targetir::rv64