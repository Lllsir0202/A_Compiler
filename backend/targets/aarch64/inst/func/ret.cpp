#include "backend/targets/aarch64/inst/func/ret.h"

namespace backend::targetir::aarch64 {
auto ARMRetInst::Emit(std::ofstream *out) -> void { *out << "    ret\n"; }

auto ARMRetInst::Create() -> ARMRetInst * {
  auto inst = new ARMRetInst();
  return inst;
}

}  // namespace backend::targetir::aarch64