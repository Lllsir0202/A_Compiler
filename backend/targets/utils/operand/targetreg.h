#ifndef BACKEND_TARGETS_UTILS_OPERAND_TARGETREG_H
#define BACKEND_TARGETS_UTILS_OPERAND_TARGETREG_H

#include <cstdint>
#include "backend/targets/utils/operand/targetoperand.h"
#include "targets/utils/operand/targetprinter.h"

namespace backend::targetir {
// Virtual register operand, before register allocation
class TargetVirtualReg : public TargetOperand {
 public:
  auto Emit(std::ofstream *out) -> void override {
    // Output the virtual register in the format "v{id}"
    *out << "v" << id_;
  }

 private:
  explicit TargetVirtualReg(TargetRegSize reg_size, TargetRegClass reg_class, size_t id)
      : TargetOperand(TargetOperandType::VirtualReg, reg_size, reg_class), id_(id) {}

  size_t id_;  // Unique ID for this virtual register
};

// Physical register operand, after register allocation
class TargetPhysicalReg : public TargetOperand {
 public:
  auto Emit(std::ofstream *out) -> void override {
    if (target_printer_ == nullptr) {
      // Should report error
      throw std::runtime_error("Target printer is not set for physical register");
    }
    // Output the physical register name using target printer
    *out << target_printer_->GetRegName(reg_size_, id_);
  }

 private:
  explicit TargetPhysicalReg(TargetRegSize reg_size, TargetRegClass reg_class, uint8_t id)
      : TargetOperand(TargetOperandType::PhysicalReg, reg_size, reg_class), id_(id) {}

  // Using ABI to specify the name of physical register,
  // i.e. 0 for eax/ax
  uint8_t id_;

  // Need to specify the architecture for physical register,
  // so that we can print the correct register name in Emit function.
  TargetPrinter *target_printer_ = nullptr;
};
}  // namespace backend::targetir
#endif