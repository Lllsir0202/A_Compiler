#include "backend/targets/aarch64/initializer/aarch64initializer.h"
#include "context/backend_context.h"
#include "targets/aarch64/phyreg/aarch64reg.h"
#include "targets/utils/operand/targetoperand.h"
#include "targets/utils/operand/targetreg.h"

namespace backend::targetir::aarch64 {
auto AArch64TargetInitializer::InitPhysicalRegPool(context::BackendContext *context) -> void {
  // In our current design
  // GPR: x0-x31(x29 is fp, x30 is lr, x31 is zero/sp)
  context->GetPhysicalRegPool().InitPhysicalRegPool(TargetRegClass::General, 32);
  // FPR: s0-s31(32)
  context->GetPhysicalRegPool().InitPhysicalRegPool(TargetRegClass::Float, 32);
  // Vector: not supported for now, we will support it when we support SVE
}

auto AArch64TargetInitializer::InitTargetPrinter() -> void {
  TargetPhysicalReg::SetTargetPrinter(&AArch64RegPrinter::GetInstance());
}

}  // namespace backend::targetir::aarch64