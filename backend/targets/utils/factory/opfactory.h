#ifndef BACKEND_TARGETS_UTILS_FACTORY_OPFACTORY_H
#define BACKEND_TARGETS_UTILS_FACTORY_OPFACTORY_H

#include "backend/targets/utils/operand/targetframe.h"
#include "backend/targets/utils/operand/targetlabel.h"
#include "backend/targets/utils/operand/targetliteral.h"
#include "backend/targets/utils/operand/targetoperand.h"
#include "backend/targets/utils/operand/targetreg.h"
namespace context {
class BackendContext;
}

namespace backend::targetir {
class OperandFactory {
  static auto CreateFrameOperand(size_t slot_id) -> TargetFrameOperand * { return new TargetFrameOperand(slot_id); }
  static auto CreateVirtualReg(TargetRegSize reg_size, TargetRegClass reg_class, size_t reg_id) -> TargetVirtualReg * {
    return new TargetVirtualReg(reg_size, reg_class, reg_id);
  }
  static auto CreatePhysicalReg(TargetRegSize reg_size, TargetRegClass reg_class, PhysicalReg *physical_reg,
                                TargetRegRole role = TargetRegRole::NONE, uint8_t offset = 0) -> TargetPhysicalReg * {
    return new TargetPhysicalReg(reg_size, reg_class, physical_reg, role, offset);
  }

  static auto CreateIntLiteral(int value) -> TargetIntLiteral * { return new TargetIntLiteral(value); }
  static auto CreateFloatLiteral(float value) -> TargetFloatLiteral * { return new TargetFloatLiteral(value); }
  static auto CreateLabelOperand(std::string label) -> TargetLabel * { return new TargetLabel(std::move(label)); }
  // Create a symbol operand,using template to support different architectures.
  template <typename SymbolType, typename... Args>
  static auto CreateSymbolOperand(Args &&...args) -> SymbolType * {
    return new SymbolType(std::forward<Args>(args)...);
  }

  friend class context::BackendContext;
};

}  // namespace backend::targetir

#endif