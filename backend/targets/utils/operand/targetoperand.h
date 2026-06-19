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
  Vector    // Vector register
};

/**
 * @brief Implement the print hint for physical register view
 */
enum class TargetRegRole : uint8_t { NONE, ZERO, STACKPOINTER, FRAMEPOINTER, RETURNADDRESS };

// Operand type
enum class TargetOperandType : uint8_t {
  VirtualReg,   // Virtual register, before register allocation
  PhysicalReg,  // Physical register, after register allocation
  Immediate,    // Immediate value
  Memory,       // Memory address
  Frame,        // Stack frame reference
  Symbol,       // Symbol reference
  Label,        // Label reference, for example, the label of a basic block
};

class TargetOperand {
 public:
  virtual ~TargetOperand() = default;
  virtual auto Emit(std::ofstream *out) -> void = 0;
  auto GetOpType() -> TargetOperandType { return type_; }
  auto IsVirtualReg() -> bool { return type_ == TargetOperandType::VirtualReg; }
  auto IsPhysicalReg() -> bool { return type_ == TargetOperandType::PhysicalReg; }
  auto IsSymbol() -> bool { return type_ == TargetOperandType::Symbol; }
  auto IsLabel() -> bool { return type_ == TargetOperandType::Label; }

 protected:
  explicit TargetOperand(TargetOperandType type) : type_(type) {}
  const TargetOperandType type_;
};

}  // namespace backend::targetir

#endif