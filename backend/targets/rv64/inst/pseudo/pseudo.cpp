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

auto RVMVInst::Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> RVMVInst * {
  auto inst = new RVMVInst();
  inst->operands_.reserve(2);
  inst->AddOperand(rs1);
  inst->AddOperand(rs2);
  inst->SetResult(rd);
  return inst;
}

}  // namespace backend::targetir::rv64