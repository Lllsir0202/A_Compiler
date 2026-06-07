#ifndef BACKEND_TARGETS_UTILS_OPERAND_TARGETREG_H
#define BACKEND_TARGETS_UTILS_OPERAND_TARGETREG_H

#include <sys/types.h>
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

  auto GetID() -> size_t { return id_; }
  auto GetRegSize() -> TargetRegSize { return reg_size_; }

 private:
  explicit TargetVirtualReg(TargetRegSize reg_size, size_t id)
      : TargetOperand(TargetOperandType::VirtualReg), id_(id), reg_size_(reg_size) {}

  size_t id_;  // Unique ID for this virtual register

  // Target register size
  TargetRegSize reg_size_;
};

// Physical register operand, after register allocation
class TargetPhysicalReg : public TargetOperand {
 public:
  auto Emit(std::ofstream *out) -> void override {
    if (target_printer == nullptr) {
      // Should report error
      throw std::runtime_error("Target printer is not set for physical register");
    }
    // Output the physical register name using target printer
    *out << target_printer->GetRegName(reg_size_, id_, reg_class_);
  }

  auto GetID() -> uint8_t { return id_; }
  auto GetRegSize() -> TargetRegSize { return reg_size_; }
  auto GetRegClass() -> TargetRegClass { return reg_class_; }
  // Called in compiler backend to set the target printer for physical register printing
  static auto SetTargetPrinter(const TargetPrinter *printer) -> void { target_printer = printer; }

 private:
  explicit TargetPhysicalReg(TargetRegSize reg_size, TargetRegClass reg_class, uint8_t id, uint8_t offset = 0)
      : TargetOperand(TargetOperandType::PhysicalReg),
        id_(id),
        reg_size_(reg_size),
        reg_class_(reg_class),
        offset_(offset) {}

  // Using ABI to specify the name of physical register,
  // i.e. 0 for eax/ax
  uint8_t id_;

  // Register size and class
  TargetRegSize reg_size_;
  TargetRegClass reg_class_;

  // Used as offset, reserved for future extension for x86
  [[maybe_unused]] uint8_t offset_ = 0;

  static const TargetPrinter *target_printer;
};
}  // namespace backend::targetir
#endif