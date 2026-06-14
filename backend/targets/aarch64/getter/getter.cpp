#include "backend/targets/aarch64/getter/getter.h"
#include "context/backend_context.h"

namespace backend::targetir::aarch64 {
auto AArch64Getter::GetStackPointerReg(context::BackendContext *context) -> TargetPhysicalReg * {
  // In armv8, the stack pointer is x31 (alias sp)
  auto x31 = context->GetPhysicalRegPool().GetPhysicalReg(TargetRegClass::General, 31);
  return context->CreatePhysicalRegView(TargetRegSize::QWord, TargetRegClass::General, x31,
                                        TargetRegRole::STACKPOINTER);
}

auto AArch64Getter::GetFramePointerReg(context::BackendContext *context) -> TargetPhysicalReg * {
  // In armv8, the frame pointer is x29 (alias fp)
  auto x29 = context->GetPhysicalRegPool().GetPhysicalReg(TargetRegClass::General, 29);
  return context->CreatePhysicalRegView(TargetRegSize::QWord, TargetRegClass::General, x29,
                                        TargetRegRole::FRAMEPOINTER);
}

auto AArch64Getter::GetLinkRegister(context::BackendContext *context) -> TargetPhysicalReg * {
  // In armv8, the link register is x30 (alias lr)
  auto x30 = context->GetPhysicalRegPool().GetPhysicalReg(TargetRegClass::General, 30);
  return context->CreatePhysicalRegView(TargetRegSize::QWord, TargetRegClass::General, x30,
                                        TargetRegRole::RETURNADDRESS);
}

auto AArch64Getter::GetZeroRegister(context::BackendContext *context, TargetRegSize size) -> TargetPhysicalReg * {
  // In armv8, the zero register is xzr (alias wzr),
  // which is encoded as 31 in instructions
  // but behaves differently based on the instruction width
  auto xzr = context->GetPhysicalRegPool().GetPhysicalReg(TargetRegClass::General, 31);
  return context->CreatePhysicalRegView(size, TargetRegClass::General, xzr, TargetRegRole::ZERO);
}

}  // namespace backend::targetir::aarch64