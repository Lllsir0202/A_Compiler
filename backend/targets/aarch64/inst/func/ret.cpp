#include "backend/targets/aarch64/inst/func/ret.h"

namespace backend::targetir::aarch64 {
auto ARMRetInst::Emit(std::ofstream *out) -> void { *out << "    ret\n"; }

auto ARMRetInst::Create(TargetOperand *rd) -> ARMRetInst * {
  auto inst = new ARMRetInst();
  inst->SetResult(rd);
  return inst;
}

}  // namespace backend::targetir::aarch64