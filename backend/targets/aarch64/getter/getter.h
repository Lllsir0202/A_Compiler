#ifndef BACKEND_TARGETS_AARCH64_GETTER_GETTER_H
#define BACKEND_TARGETS_AARCH64_GETTER_GETTER_H

#include "backend/targets/utils/manager/registerpool.h"
#include "backend/targets/utils/operand/targetreg.h"
namespace backend::targetir::aarch64 {
// Simplify the getting of some specific registers
class AArch64Getter {
 public:
  ~AArch64Getter() = default;

  // Get the stack pointer register
  auto GetStackPointerReg(context::BackendContext *context) -> TargetPhysicalReg *;

  // Get the frame pointer register
  auto GetFramePointerReg(context::BackendContext *context) -> TargetPhysicalReg *;

  // Get the link register
  auto GetLinkRegister(context::BackendContext *context) -> TargetPhysicalReg *;

  // Get the zero register
  auto GetZeroRegister(context::BackendContext *context, TargetRegSize size) -> TargetPhysicalReg *;
};

}  // namespace backend::targetir::aarch64
#endif