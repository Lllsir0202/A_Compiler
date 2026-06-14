#include "backend/targets/aarch64/initializer/aarch64initializer.h"
#include <unordered_set>
#include "context/backend_context.h"
#include "targets/aarch64/phyreg/aarch64reg.h"
#include "targets/utils/manager/registerpool.h"
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

auto AArch64TargetInitializer::InitRAManager(context::BackendContext *context, int optimization_level) -> void {
  // For armv8, reserved registers are lr(x30), sp/zr(x31), and fp(x29)
  // Means we are in optimization level 1 or above, we can use fp
  // For armv8, reserved registers are lr(x30) and sp/zr(x31)
  std::unordered_set<PhysicalReg *> reserved_regs = {
      context->GetPhysicalRegPool().GetPhysicalReg(TargetRegClass::General, 30),  // lr
      context->GetPhysicalRegPool().GetPhysicalReg(TargetRegClass::General, 31)   // sp/zr
  };
  if (optimization_level == 0) {
    reserved_regs.insert(context->GetPhysicalRegPool().GetPhysicalReg(TargetRegClass::General, 29));  // fp
  }
  context->GetRAManager().SetReservedRegs(reserved_regs);

  // Caller-saved registers: x0-x18 (x19-x28 are callee-saved)
  std::unordered_set<PhysicalReg *> caller_saved_regs;
  for (int i = 0; i <= 18; ++i) {
    caller_saved_regs.insert(context->GetPhysicalRegPool().GetPhysicalReg(TargetRegClass::General, i));
  }
  context->GetRAManager().SetCallerSavedRegs(caller_saved_regs);
  // Callee-saved registers: x19-x28 (x0-x18 are caller-saved)
  std::unordered_set<PhysicalReg *> callee_saved_regs;
  for (int i = 19; i <= 28; ++i) {
    callee_saved_regs.insert(context->GetPhysicalRegPool().GetPhysicalReg(TargetRegClass::General, i));
  }
  context->GetRAManager().SetCalleeSavedRegs(callee_saved_regs);
}

}  // namespace backend::targetir::aarch64