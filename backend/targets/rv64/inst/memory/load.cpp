#include "backend/targets/rv64/inst/memory/load.h"
#include "backend/targets/utils/operand/targetoperand.h"

namespace backend::targetir::rv64 {
auto RVLWInst::Emit(std::ofstream *out) -> void {
  *out << "    lw ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << "(";
  operands_[1]->Emit(out);
  *out << ")\n";
}

auto RVLWInst::Create(TargetOperand *rd, TargetOperand *imm, TargetOperand *rs) -> RVLWInst * {
  auto *inst = new RVLWInst();
  inst->operands_.reserve(2);
  inst->AddOperand(imm);
  inst->AddOperand(rs);
  inst->SetResult(rd);
  return inst;
}

auto RVFLWInst::Emit(std::ofstream *out) -> void {
  *out << "    flw ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << "(";
  operands_[1]->Emit(out);
  *out << ")\n";
}

auto RVFLWInst::Create(TargetOperand *rd, TargetOperand *imm, TargetOperand *rs) -> RVFLWInst * {
  auto *inst = new RVFLWInst();
  inst->operands_.reserve(2);
  inst->AddOperand(imm);
  inst->AddOperand(rs);
  inst->SetResult(rd);
  return inst;
}

auto RVLDInst::Emit(std::ofstream *out) -> void {
  *out << "    ld ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << "(";
  operands_[1]->Emit(out);
  *out << ")\n";
}

auto RVLDInst::Create(TargetOperand *rd, TargetOperand *imm, TargetOperand *rs) -> RVLDInst * {
  auto *inst = new RVLDInst();
  inst->operands_.reserve(2);
  inst->AddOperand(imm);
  inst->AddOperand(rs);
  inst->SetResult(rd);
  return inst;
}

auto RVFLDInst::Emit(std::ofstream *out) -> void {
  *out << "    fld ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << "(";
  operands_[1]->Emit(out);
  *out << ")\n";
}

auto RVFLDInst::Create(TargetOperand *rd, TargetOperand *imm, TargetOperand *rs) -> RVFLDInst * {
  auto *inst = new RVFLDInst();
  inst->operands_.reserve(2);
  inst->AddOperand(imm);
  inst->AddOperand(rs);
  inst->SetResult(rd);
  return inst;
}

}  // namespace backend::targetir::rv64