#include "backend/targets/aarch64/inst/branch/branch.h"
#include "backend/targets/utils/operand/targetoperand.h"

namespace backend::targetir::aarch64 {
auto ARMBInst::Emit(std::ofstream *out) -> void {
  *out << "    b ";
  operands_[0]->Emit(out);
  *out << "\n";
}

auto ARMBInst::Create(TargetOperand *label) -> ARMBInst * {
  auto inst = new ARMBInst();
  inst->operands_.reserve(1);
  inst->operands_.emplace_back(label);
  return inst;
}

// branch on condition
auto ARMBCondInst::Emit(std::ofstream *out) -> void {
  *out << "    b.";
  *out << CondOpToString(op_) << " ";
  operands_[0]->Emit(out);
  *out << "\n";
}

auto ARMBCondInst::Create(ARMBCondOp op, TargetOperand *label) -> ARMBCondInst * {
  auto inst = new ARMBCondInst();
  inst->op_ = op;
  inst->operands_.reserve(1);
  inst->operands_.emplace_back(label);
  return inst;
}

}  // namespace backend::targetir::aarch64