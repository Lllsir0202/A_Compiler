#include "backend/targets/rv64/inst/branch/jmp.h"
#include "backend/targets/utils/operand/targetoperand.h"
namespace backend::targetir::rv64 {
auto RVJMPInst::Emit(std::ofstream *out) -> void {
  *out << "    j ";
  operands_[0]->Emit(out);
  *out << '\n';
}

auto RVJMPInst::Create(TargetOperand *target) -> RVJMPInst * {
  auto *inst = new RVJMPInst();
  inst->operands_.reserve(1);
  inst->AddOperand(target);
  return inst;
}

auto RVJALInst::Emit(std::ofstream *out) -> void {
  *out << "    jal ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << '\n';
}

auto RVJALInst::Create(TargetOperand *rd, TargetOperand *target) -> RVJALInst * {
  auto *inst = new RVJALInst();
  inst->operands_.reserve(1);
  inst->AddOperand(target);
  inst->SetResult(rd);
  return inst;
}

auto RVJALRInst::Emit(std::ofstream *out) -> void {
  *out << "    jalr ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << '\n';
}

auto RVJALRInst::Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *imm) -> RVJALRInst * {
  auto *inst = new RVJALRInst();
  inst->operands_.reserve(2);
  inst->AddOperand(rs1);
  inst->AddOperand(imm);
  inst->SetResult(rd);
  return inst;
}

// Branch instructions
auto RVBEQInst::Emit(std::ofstream *out) -> void {
  *out << "    beq ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << '\n';
}

auto RVBEQInst::Create(TargetOperand *rd, TargetOperand *target) -> RVBEQInst * {
  auto *inst = new RVBEQInst();
  inst->operands_.reserve(1);
  inst->AddOperand(target);
  inst->SetResult(rd);
  return inst;
}

auto RVBNEInst::Emit(std::ofstream *out) -> void {
  *out << "    bne ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << '\n';
}

auto RVBNEInst::Create(TargetOperand *rd, TargetOperand *target) -> RVBNEInst * {
  auto *inst = new RVBNEInst();
  inst->operands_.reserve(1);
  inst->AddOperand(target);
  inst->SetResult(rd);
  return inst;
}

auto RVBLTInst::Emit(std::ofstream *out) -> void {
  *out << "    blt ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << '\n';
}

auto RVBLTInst::Create(TargetOperand *rd, TargetOperand *target) -> RVBLTInst * {
  auto *inst = new RVBLTInst();
  inst->operands_.reserve(1);
  inst->AddOperand(target);
  inst->SetResult(rd);
  return inst;
}

auto RVBGEInst::Emit(std::ofstream *out) -> void {
  *out << "    bge ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << '\n';
}

auto RVBGEInst::Create(TargetOperand *rd, TargetOperand *target) -> RVBGEInst * {
  auto *inst = new RVBGEInst();
  inst->operands_.reserve(1);
  inst->AddOperand(target);
  inst->SetResult(rd);
  return inst;
}

}  // namespace backend::targetir::rv64