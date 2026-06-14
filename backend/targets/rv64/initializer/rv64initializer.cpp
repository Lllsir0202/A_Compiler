#include "backend/targets/rv64/initializer/rv64initializer.h"
#include <unordered_set>
#include "backend/targets/rv64/phyreg/rv64reg.h"
#include "backend/targets/utils/operand/targetoperand.h"
#include "context/backend_context.h"
#include "targets/utils/manager/registerpool.h"
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

auto RV64TargetInitializer::InitRAManager(context::BackendContext *context, int optimization_level) -> void {
  // If no-opt, fp is reserved, otherwise, we will use fp as a gpr

  // For rv64, reserved registers are zero(x0), ra(x1), sp(x2), gp(x3), tp(x4), and fp(x8)
  // If optimization is enabled, we can use fp as a general-purpose register,
  // so only zero(x0), ra(x1), sp(x2), gp(x3), and tp(x4) are reserved

  std::unordered_set<PhysicalReg *> reserved_regs;
  for (int i = 0; i <= 4; ++i) {
    reserved_regs.insert(context->GetPhysicalRegPool().GetPhysicalReg(TargetRegClass::General, i));
  }
  if (optimization_level == 0) {
    reserved_regs.insert(context->GetPhysicalRegPool().GetPhysicalReg(TargetRegClass::General, 8));  // fp
  }
  context->GetRAManager().SetReservedRegs(reserved_regs);

  // Caller-saved registers: x1, x5-x7, x10-x17, x28-x31, f0-f7, f10-f17, f28-f31
  std::unordered_set<PhysicalReg *> caller_saved_regs = {
      context->GetPhysicalRegPool().GetPhysicalReg(TargetRegClass::General, 1)};

  for (int i = 5; i <= 7; ++i) {
    caller_saved_regs.insert(context->GetPhysicalRegPool().GetPhysicalReg(TargetRegClass::General, i));
  }
  for (int i = 10; i <= 17; ++i) {
    caller_saved_regs.insert(context->GetPhysicalRegPool().GetPhysicalReg(TargetRegClass::General, i));
  }
  for (int i = 28; i <= 31; ++i) {
    caller_saved_regs.insert(context->GetPhysicalRegPool().GetPhysicalReg(TargetRegClass::General, i));
  }
  for (int i = 0; i <= 7; ++i) {
    caller_saved_regs.insert(context->GetPhysicalRegPool().GetPhysicalReg(TargetRegClass::Float, i));
  }
  for (int i = 10; i <= 17; ++i) {
    caller_saved_regs.insert(context->GetPhysicalRegPool().GetPhysicalReg(TargetRegClass::Float, i));
  }
  for (int i = 28; i <= 31; ++i) {
    caller_saved_regs.insert(context->GetPhysicalRegPool().GetPhysicalReg(TargetRegClass::Float, i));
  }

  context->GetRAManager().SetCallerSavedRegs(caller_saved_regs);

  // callee-saved registers: x2, x8, x9, x18-x27, f8-f9, f18-f27
  std::unordered_set<PhysicalReg *> callee_saved_regs = {
      context->GetPhysicalRegPool().GetPhysicalReg(TargetRegClass::General, 2)};

  for (int i = 8; i <= 9; ++i) {
    callee_saved_regs.insert(context->GetPhysicalRegPool().GetPhysicalReg(TargetRegClass::General, i));
  }

  for (int i = 18; i <= 27; ++i) {
    callee_saved_regs.insert(context->GetPhysicalRegPool().GetPhysicalReg(TargetRegClass::General, i));
  }
  for (int i = 8; i <= 9; ++i) {
    callee_saved_regs.insert(context->GetPhysicalRegPool().GetPhysicalReg(TargetRegClass::Float, i));
  }
  for (int i = 18; i <= 27; ++i) {
    callee_saved_regs.insert(context->GetPhysicalRegPool().GetPhysicalReg(TargetRegClass::Float, i));
  }
  context->GetRAManager().SetCalleeSavedRegs(callee_saved_regs);

  // Set up free registers based on the reserved registers and the physical register pool
  context->GetRAManager().InitializeFreeRegs(context);
}

}  // namespace backend::targetir::rv64