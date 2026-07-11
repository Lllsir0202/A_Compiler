#include "backend/targets/rv64/getter/getter.h"
#include "context/backend_context.h"

namespace backend::targetir::rv64 {
auto RV64Getter::GetStackPointerReg(context::BackendContext *context) -> TargetPhysicalReg * {
  // In RISC-V, the stack pointer is x2 (alias sp)
  auto x2 = context->GetPhysicalRegPool().GetPhysicalReg(TargetRegClass::General, 2);
  return context->CreatePhysicalRegView(TargetRegSize::DWord, TargetRegClass::General, x2, TargetRegRole::STACKPOINTER);
}

auto RV64Getter::GetFramePointerReg(context::BackendContext *context) -> TargetPhysicalReg * {
  // In RISC-V, the frame pointer is x8 (alias s0 or fp)
  auto x8 = context->GetPhysicalRegPool().GetPhysicalReg(TargetRegClass::General, 8);
  return context->CreatePhysicalRegView(TargetRegSize::DWord, TargetRegClass::General, x8, TargetRegRole::FRAMEPOINTER);
}

auto RV64Getter::GetReturnAddressReg(context::BackendContext *context) -> TargetPhysicalReg * {
  // In RISC-V, the return address register is x1 (alias ra)
  auto x1 = context->GetPhysicalRegPool().GetPhysicalReg(TargetRegClass::General, 1);
  return context->CreatePhysicalRegView(TargetRegSize::DWord, TargetRegClass::General, x1,
                                        TargetRegRole::RETURNADDRESS);
}

auto RV64Getter::GetZeroRegister(context::BackendContext *context, TargetRegSize size) -> TargetPhysicalReg * {
  // In RISC-V, the zero register is x0 (alias zero),
  // which always reads as zero and ignores writes
  auto x0 = context->GetPhysicalRegPool().GetPhysicalReg(TargetRegClass::General, 0);
  return context->CreatePhysicalRegView(size, TargetRegClass::General, x0, TargetRegRole::ZERO);
}

auto RV64Getter::GetReturnValueReg(context::BackendContext *context, TargetRegSize size) -> TargetPhysicalReg * {
  // In RISC-V, the return value register is x10 (alias a0)
  auto x10 = context->GetPhysicalRegPool().GetPhysicalReg(TargetRegClass::General, 10);
  return context->CreatePhysicalRegView(size, TargetRegClass::General, x10, TargetRegRole::NONE);
}

auto RV64Getter::GetFloatReturnValueReg(context::BackendContext *context, TargetRegSize size) -> TargetPhysicalReg * {
  // In RISC-V, the floating-point return value register is f10 (alias fa0)
  auto f10 = context->GetPhysicalRegPool().GetPhysicalReg(TargetRegClass::Float, 10);
  return context->CreatePhysicalRegView(size, TargetRegClass::Float, f10, TargetRegRole::NONE);
}

}  // namespace backend::targetir::rv64
