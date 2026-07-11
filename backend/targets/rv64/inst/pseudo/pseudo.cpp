#include "backend/targets/rv64/inst/pseudo/pseudo.h"
#include "backend/targets/utils/operand/targetoperand.h"

namespace backend::targetir::rv64 {
auto RVLIInst::Emit(std::ofstream *out) -> void {
  *out << "    li ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << '\n';
}

auto RVLIInst::Create(TargetOperand *rd, TargetOperand *imm) -> RVLIInst * {
  auto inst = new RVLIInst();
  inst->operands_.reserve(1);
  inst->AddOperand(imm);
  inst->SetResult(rd);
  return inst;
}

auto RVMVInst::Emit(std::ofstream *out) -> void {
  *out << "    mv ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << '\n';
}

auto RVMVInst::Create(TargetOperand *rd, TargetOperand *rs) -> RVMVInst * {
  auto inst = new RVMVInst();
  inst->operands_.reserve(1);
  inst->AddOperand(rs);
  inst->SetResult(rd);
  return inst;
}

auto RVFMVInst::Emit(std::ofstream *out) -> void {
  *out << "    fmv.s ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << '\n';
}

auto RVFMVInst::Create(TargetOperand *rd, TargetOperand *rs) -> RVFMVInst * {
  auto inst = new RVFMVInst();
  inst->operands_.reserve(1);
  inst->AddOperand(rs);
  inst->SetResult(rd);
  return inst;
}

auto RVLAInst::Emit(std::ofstream *out) -> void {
  *out << "    la ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << '\n';
}

auto RVLAInst::Create(TargetOperand *rd, TargetOperand *symbol) -> RVLAInst * {
  auto inst = new RVLAInst();
  inst->operands_.reserve(1);
  inst->AddOperand(symbol);
  inst->SetResult(rd);
  return inst;
}

}  // namespace backend::targetir::rv64
