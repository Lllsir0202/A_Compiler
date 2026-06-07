#ifndef BACKEND_TARGETS_UTILS_FACTORY_OPFACTORY_H
#define BACKEND_TARGETS_UTILS_FACTORY_OPFACTORY_H

#include "backend/targets/utils/operand/targetframe.h"
#include "backend/targets/utils/operand/targetliteral.h"
#include "backend/targets/utils/operand/targetreg.h"
namespace context {
class BackendContext;
}

namespace backend::targetir {
class OperandFactory {
  static auto CreateFrameOperand(size_t slot_id) -> TargetFrameOperand * { return new TargetFrameOperand(slot_id); }
  static auto CreateVirtualReg(TargetRegSize reg_size, size_t reg_id) -> TargetVirtualReg * {
    return new TargetVirtualReg(reg_size, reg_id);
  }
  static auto CreatePhysicalReg(TargetRegSize reg_size, TargetRegClass reg_class, uint8_t reg_id,
                                uint8_t offset = 0) -> TargetPhysicalReg * {
    return new TargetPhysicalReg(reg_size, reg_class, reg_id, offset);
  }

  static auto CreateIntLiteral(int value) -> TargetIntLiteral * { return new TargetIntLiteral(value); }
  static auto CreateFloatLiteral(float value) -> TargetFloatLiteral * { return new TargetFloatLiteral(value); }
  friend class context::BackendContext;
};

}  // namespace backend::targetir

#endif