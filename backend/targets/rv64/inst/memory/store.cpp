#include "backend/targets/rv64/inst/memory/store.h"
#include "backend/targets/utils/operand/targetoperand.h"

namespace backend::targetir::rv64 {
auto RVSWInst::Emit(std::ofstream *out) -> void {
  *out << "    sw ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << ", ";
  operands_[2]->Emit(out);
  *out << '\n';
}

auto RVSWInst::Create(TargetOperand *value, TargetOperand *imm, TargetOperand *rs) -> RVSWInst * {
  auto *inst = new RVSWInst();
  inst->operands_.reserve(3);
  inst->AddOperand(value);
  inst->AddOperand(imm);
  inst->AddOperand(rs);
  return inst;
}

auto RVFSWInst::Emit(std::ofstream *out) -> void {
  *out << "    fsw ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << "(";
  operands_[2]->Emit(out);
  *out << ")\n";
}

auto RVFSWInst::Create(TargetOperand *value, TargetOperand *imm, TargetOperand *rs) -> RVFSWInst * {
  auto *inst = new RVFSWInst();
  inst->operands_.reserve(3);
  inst->AddOperand(value);
  inst->AddOperand(imm);
  inst->AddOperand(rs);
  return inst;
}

auto RVSDInst::Emit(std::ofstream *out) -> void {
  *out << "    sd ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << "(";
  operands_[2]->Emit(out);
  *out << ")\n";
}

auto RVSDInst::Create(TargetOperand *value, TargetOperand *imm, TargetOperand *rs) -> RVSDInst * {
  auto *inst = new RVSDInst();
  inst->operands_.reserve(3);
  inst->AddOperand(value);
  inst->AddOperand(imm);
  inst->AddOperand(rs);
  return inst;
}

auto RVFSDInst::Emit(std::ofstream *out) -> void {
  *out << "    fsd ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << "(";
  operands_[2]->Emit(out);
  *out << ")\n";
}

auto RVFSDInst::Create(TargetOperand *value, TargetOperand *imm, TargetOperand *rs) -> RVFSDInst * {
  auto *inst = new RVFSDInst();
  inst->operands_.reserve(3);
  inst->AddOperand(value);
  inst->AddOperand(imm);
  inst->AddOperand(rs);
  return inst;
}

}  // namespace backend::targetir::rv64