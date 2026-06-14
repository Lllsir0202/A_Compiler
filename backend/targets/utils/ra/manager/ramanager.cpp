#include "backend/targets/utils/ra/manager/ramanager.h"
#include "context/backend_context.h"

namespace backend::targetir {
auto RAManager::InitializeFreeRegs(context::BackendContext *context) -> void {
  // Initialize free registers based on the reserved registers and the physical register pool
  auto &physical_reg_pool = context->GetPhysicalRegPool();
  for (const auto &reg : physical_reg_pool.GetPhysicalRegs(TargetRegClass::General)) {
    if (!IsReservedReg(reg)) {
      free_gpr_physical_regs_.insert(reg);
    }
  }
  for (const auto &reg : physical_reg_pool.GetPhysicalRegs(TargetRegClass::Float)) {
    if (!IsReservedReg(reg)) {
      free_fpr_physical_regs_.insert(reg);
    }
  }
}

}  // namespace backend::targetir