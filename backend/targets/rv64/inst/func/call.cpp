#include "backend/targets/rv64/inst/func/call.h"

namespace backend::targetir::rv64 {
auto RVCALLInst::Emit(std::ofstream *out) -> void { *out << "    call " << callee_ << '\n'; }

auto RVCALLInst::Create(std::string callee, const std::vector<TargetOperand *> &args) -> RVCALLInst * {
  auto inst = new RVCALLInst();
  inst->callee_ = std::move(callee);
  for (auto arg : args) {
    inst->AddOperand(arg);
  }
  return inst;
}

}  // namespace backend::targetir::rv64