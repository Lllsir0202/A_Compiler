#include "backend/targets/aarch64/inst/memory/adrp.h"
#include "backend/targets/utils/operand/targetoperand.h"

namespace backend::targetir::aarch64 {
auto ARMAdrpInst::Emit(std::ofstream *out) -> void {
  *out << "    adrp ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << "\n";
}

auto ARMAdrpInst::Create(TargetOperand *dst, TargetOperand *label) -> ARMAdrpInst * {
  auto *inst = new ARMAdrpInst();
  inst->operands_.reserve(1);
  inst->AddOperand(label);
  inst->SetResult(dst);
  return inst;
}

}  // namespace backend::targetir::aarch64