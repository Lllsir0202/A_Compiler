#include "backend/targets/rv64/initializer/rv64initializer.h"
#include "backend/targets/rv64/phyreg/rv64reg.h"
#include "backend/targets/utils/operand/targetoperand.h"
#include "context/backend_context.h"
namespace backend::targetir::rv64 {
auto RV64TargetInitializer::InitPhysicalRegPool(context::BackendContext *context) -> void {
  // In our current design
  // GPR: x0-x31(32)
  context->GetPhysicalRegPool().InitPhysicalRegPool(TargetRegClass::General, 32);
  // FPR: s0-s31(32)
  context->GetPhysicalRegPool().InitPhysicalRegPool(TargetRegClass::Float, 32);
  // Vector: not supported for now, we will support it when we support SVE
}

auto RV64TargetInitializer::InitTargetPrinter() -> void {
  TargetPhysicalReg::SetTargetPrinter(&RV64RegPrinter::GetInstance());
}

}  // namespace backend::targetir::rv64