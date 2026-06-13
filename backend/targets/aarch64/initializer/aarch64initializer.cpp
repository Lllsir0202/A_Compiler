#include "backend/targets/aarch64/initializer/aarch64initializer.h"
#include "context/backend_context.h"
#include "targets/utils/operand/targetoperand.h"

namespace backend::targetir::aarch64 {
auto AArch64TargetInitializer::InitPhysicalRegPool() -> void {
  // In our current design
  // GPR: x0-x30(31)
  context_->GetPhysicalRegPool().InitPhysicalRegPool(TargetRegClass::General, 31);
  // FPR: s0-s31(32)
  context_->GetPhysicalRegPool().InitPhysicalRegPool(TargetRegClass::Float, 32);
  // Special: zero register, stack pointer, frame pointer, link register
  context_->GetPhysicalRegPool().InitPhysicalRegPool(TargetRegClass::Special, 4);
  // Vector: not supported for now, we will support it when we support SVE
}

}  // namespace backend::targetir::aarch64