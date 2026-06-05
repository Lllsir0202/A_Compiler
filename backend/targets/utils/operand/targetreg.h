#ifndef BACKEND_TARGETS_UTILS_OPERAND_TARGETREG_H
#define BACKEND_TARGETS_UTILS_OPERAND_TARGETREG_H

#include <cstdint>
#include "backend/targets/utils/operand/targetoperand.h"
#include "targets/utils/operand/targetmap.h"

namespace backend::targetir {
// Virtual register operand, before register allocation
class TargetVirtualReg : public TargetOperand {
 public:
  auto Emit(std::ofstream *out) -> void override {
    // Output the virtual register in the format "v{id}"
    *out << "v" << id_;
  }

 private:
  explicit TargetVirtualReg(TargetRegSize reg_size, size_t id)
      : TargetOperand(TargetOperandType::VirtualReg, reg_size), id_(id) {}

  size_t id_;  // Unique ID for this virtual register
};

// Physical register operand, after register allocation
class TargetPhysicalReg : public TargetOperand {
 public:
  auto Emit(std::ofstream *out) -> void override {
    if (target_map_ == nullptr) {
      // Should report error
      throw std::runtime_error("Target map is not set for physical register");
    }
    // Output the physical register name using target map
    *out << target_map_->GetRegName(reg_size_, id_);
  }

 private:
  explicit TargetPhysicalReg(TargetRegSize reg_size, uint8_t id)
      : TargetOperand(TargetOperandType::PhysicalReg, reg_size), id_(id) {}

  // Using ABI to specify the name of physical register,
  // i.e. 0 for eax/ax
  uint8_t id_;

  // Need to specify the architecture for physical register,
  // so that we can print the correct register name in Emit function.
  TargetMap *target_map_ = nullptr;
};
}  // namespace backend::targetir
#endif