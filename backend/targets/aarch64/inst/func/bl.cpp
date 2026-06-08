#include "backend/targets/aarch64/inst/func/bl.h"
#include "backend/targets/utils/operand/targetoperand.h"
namespace backend::targetir::aarch64 {
auto ARMBLInst::Emit(std::ofstream *out) -> void {
  *out << "    bl ";
  operands_[0]->Emit(out);
  *out << "\n";
}

auto ARMBLInst::Create(TargetOperand *label) -> ARMBLInst * {
  auto inst = new ARMBLInst();
  inst->operands_.reserve(1);
  inst->AddOperand(label);
  return inst;
}

}  // namespace backend::targetir::aarch64