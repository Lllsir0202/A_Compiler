#include "backend/targets/aarch64/initializer/aarch64initializer.h"
#include "context/backend_context.h"
#include "targets/utils/operand/targetoperand.h"

namespace backend::targetir::aarch64 {
auto AArch64TargetInitializer::InitPhysicalRegPool() -> void {
  // In our current design
  // GPR: x0-x31(x29 is fp, x30 is lr, x31 is zero/sp)
  context_->GetPhysicalRegPool().InitPhysicalRegPool(TargetRegClass::General, 32);
  // FPR: s0-s31(32)
  context_->GetPhysicalRegPool().InitPhysicalRegPool(TargetRegClass::Float, 32);
  // Vector: not supported for now, we will support it when we support SVE
}

}  // namespace backend::targetir::aarch64