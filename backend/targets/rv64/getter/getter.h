#ifndef BACKEND_TARGETS_RV64_GETTER_GETTER_H
#define BACKEND_TARGETS_RV64_GETTER_GETTER_H

#include "backend/targets/utils/manager/registerpool.h"
#include "backend/targets/utils/operand/targetreg.h"
namespace backend::targetir::rv64 {
class RV64Getter {
 public:
  ~RV64Getter() = default;

  // Get the stack pointer register
  auto GetStackPointerReg(context::BackendContext *context) -> TargetPhysicalReg *;

  // Get the frame pointer register
  auto GetFramePointerReg(context::BackendContext *context) -> TargetPhysicalReg *;

  // Get the return address register
  auto GetReturnAddressReg(context::BackendContext *context) -> TargetPhysicalReg *;

  // Get the zero register
  auto GetZeroRegister(context::BackendContext *context, TargetRegSize size) -> TargetPhysicalReg *;

  // Get the return value register
  auto GetReturnValueReg(context::BackendContext *context, TargetRegSize size) -> TargetPhysicalReg *;
};

}  // namespace backend::targetir::rv64

#endif