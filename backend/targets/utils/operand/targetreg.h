#ifndef BACKEND_TARGETS_UTILS_OPERAND_TARGETREG_H
#define BACKEND_TARGETS_UTILS_OPERAND_TARGETREG_H

#include <sys/types.h>
#include <cstdint>
#include "backend/targets/utils/operand/targetoperand.h"
#include "targets/utils/operand/targetprinter.h"

namespace backend::targetir {
class PhysicalReg;

// Virtual register operand, before register allocation
class TargetVirtualReg : public TargetOperand {
 public:
  auto Emit(std::ofstream *out) -> void override {
    // Output the virtual register in the format "v{id}"
    if (reg_class_ == TargetRegClass::General) {
      *out << "v" << id_ << "_gpr";
    } else if (reg_class_ == TargetRegClass::Float) {
      *out << "v" << id_ << "_fpr";
    } else if (reg_class_ == TargetRegClass::Vector) {
      *out << "v" << id_ << "_vec";
    } else {
      // Should report error
      throw std::runtime_error("Unknown register class for virtual register");
    }
    *out << "v" << id_;
  }

  auto GetID() -> size_t { return id_; }
  auto GetRegSize() -> TargetRegSize { return reg_size_; }
  auto GetRegClass() -> TargetRegClass { return reg_class_; }

 private:
  explicit TargetVirtualReg(TargetRegSize reg_size, TargetRegClass reg_class, size_t id)
      : TargetOperand(TargetOperandType::VirtualReg), id_(id), reg_size_(reg_size), reg_class_(reg_class) {}
  friend class OperandFactory;
  size_t id_;  // Unique ID for this virtual register

  // Target register size
  TargetRegSize reg_size_;

  // Record GPR or FPR, used for register allocation
  TargetRegClass reg_class_;
};

// Physical register operand, after register allocation
/**
 * @note Note that this class is just a view of the concrete physical register
 */
class TargetPhysicalReg : public TargetOperand {
 public:
  auto Emit(std::ofstream *out) -> void override;

  auto GetID() -> uint8_t;
  auto GetRegSize() -> TargetRegSize { return reg_size_; }
  auto GetRegClass() -> TargetRegClass { return reg_class_; }
  // Called in compiler backend to set the target printer for physical register printing
  static auto SetTargetPrinter(const TargetRegPrinter *printer) -> void { target_printer = printer; }
  auto GetRole() -> TargetRegRole { return role_; }

 private:
  explicit TargetPhysicalReg(TargetRegSize reg_size, TargetRegClass reg_class, PhysicalReg *physical_reg,
                             TargetRegRole role = TargetRegRole::NONE, uint8_t offset = 0)
      : TargetOperand(TargetOperandType::PhysicalReg),
        physical_reg_(physical_reg),
        reg_size_(reg_size),
        reg_class_(reg_class),
        offset_(offset),
        role_(role) {}
  friend class OperandFactory;

  // Concrete physical register, managed by the physical register pool
  PhysicalReg *physical_reg_;

  // Register size and class
  TargetRegSize reg_size_;
  TargetRegClass reg_class_;

  // Used as offset, reserved for future extension for x86
  [[maybe_unused]] uint8_t offset_ = 0;

  static const TargetRegPrinter *target_printer;

  TargetRegRole role_;  // Just for print hint, and only for scalar registers.
};

}  // namespace backend::targetir
#endif