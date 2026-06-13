#include "backend/targets/rv64/initializer/rv64initializer.h"
#include "context/backend_context.h"
#include "targets/utils/operand/targetoperand.h"

namespace backend::targetir::rv64 {
auto RV64TargetInitializer::InitPhysicalRegPool() -> void {
  // In our current design
  // GPR: 
  context_->GetPhysicalRegPool().InitPhysicalRegPool(TargetRegClass::General, 31);
  // FPR: s0-s31(32)
  context_->GetPhysicalRegPool().InitPhysicalRegPool(TargetRegClass::Float, 32);
  // Vector: not supported for now, we will support it when we support SVE
}

}  // namespace backend::targetir::rv64