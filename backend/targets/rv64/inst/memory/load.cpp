#include "backend/targets/rv64/inst/memory/load.h"
#include "backend/targets/utils/operand/targetoperand.h"

namespace backend::targetir::rv64 {
auto RVLWInst::Emit(std::ofstream *out) -> void {
  *out << "    lw ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << '\n';
}

auto RVLWInst::Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *imm) -> RVLWInst * {
  auto *inst = new RVLWInst();
  inst->operands_.reserve(2);
  inst->AddOperand(rs1);
  inst->AddOperand(imm);
  inst->SetResult(rd);
  return inst;
}

auto RVFLWInst::Emit(std::ofstream *out) -> void {
  *out << "    flw ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << '\n';
}

auto RVFLWInst::Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *imm) -> RVFLWInst * {
  auto *inst = new RVFLWInst();
  inst->operands_.reserve(2);
  inst->AddOperand(rs1);
  inst->AddOperand(imm);
  inst->SetResult(rd);
  return inst;
}

}  // namespace backend::targetir::rv64