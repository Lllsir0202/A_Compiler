#include "backend/targets/rv64/inst/func/ret.h"
#include "backend/targets/utils/operand/targetoperand.h"

namespace backend::targetir::rv64 {
auto RVRETInst::Emit(std::ofstream *out) -> void { *out << "    ret\n"; }

auto RVRETInst::Create(TargetOperand *operand) -> RVRETInst * {
  auto *inst = new RVRETInst();
  inst->AddOperand(operand);
  return inst;
}

auto RVRETInst::Create() -> RVRETInst * {
  auto *inst = new RVRETInst();
  return inst;
}

}  // namespace backend::targetir::rv64