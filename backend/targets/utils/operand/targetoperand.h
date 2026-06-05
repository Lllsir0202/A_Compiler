#ifndef BACKEND_TARGETS_UTILS_OPERAND_TARGETOPERAND_H
#define BACKEND_TARGETS_UTILS_OPERAND_TARGETOPERAND_H

#include <cstdint>
#include <fstream>
namespace backend::targetir {

// Register operand size
enum class TargetRegSize : uint8_t {
  Byte = 1,   // 8-bit register
  Word = 2,   // 16-bit register
  DWord = 4,  // 32-bit register
  QWord = 8,  // 64-bit register
};

// Register class
enum class TargetRegClass : uint8_t {
  General,  // General-purpose register
  Float,    // Floating-point register
  Vector,   // Vector register
  Special,  // Special-purpose register
};

// Operand type
enum class TargetOperandType : uint8_t {
  VirtualReg,   // Virtual register, before register allocation
  PhysicalReg,  // Physical register, after register allocation
  Immediate,    // Immediate value
  Memory,       // Memory address
  Frame,        // Stack frame reference
};

class TargetOperand {
 public:
  virtual auto Emit(std::ofstream *out) -> void = 0;
  auto GetOpType() -> TargetOperandType { return type_; }
  auto IsVirtualReg() -> bool { return type_ == TargetOperandType::VirtualReg; }
  auto IsPhysicalReg() -> bool { return type_ == TargetOperandType::PhysicalReg; }
  auto GetRegSize() -> TargetRegSize { return reg_size_; }
  auto GetRegClass() -> TargetRegClass { return reg_class_; }

 protected:
  explicit TargetOperand(TargetOperandType type, TargetRegSize reg_size, TargetRegClass reg_class)
      : type_(type), reg_size_(reg_size), reg_class_(reg_class) {}
  const TargetOperandType type_;
  // We might also need to record the register size
  const TargetRegSize reg_size_;
  // We might also need to record the register class
  const TargetRegClass reg_class_;
};

}  // namespace backend::targetir

#endif