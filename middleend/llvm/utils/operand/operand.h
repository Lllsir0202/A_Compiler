/**
 * @brief This file defines the operand for llvm ir
 * @note All operand should inherit from this class,
 * and implement the Emit() function to emit the operand as a string,
 * moreover, all of them should only be created via a factory.
 */
#ifndef MIDDLEEND_LLVM_UTILS_OPERAND_OPERAND_H
#define MIDDLEEND_LLVM_UTILS_OPERAND_OPERAND_H
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <string>
#include "utils/type/type.h"

// Forward declare the operand factory to allow it to create operands
namespace middleend::llvm::utils::factory {
class OperandFactory;
}

namespace middleend::llvm::inst {
class Instruction;
}

namespace middleend::llvm::utils {
enum OpType : uint8_t { Register, Literal, Memory, Label, Global };

class Operand {
 public:
  // Do not allow the operand change its optype after creation
  // Interface to get the operand type
  auto GetOpType() const -> OpType { return op_type_; }

  // Interface to emit the operand
  virtual auto Emit(std::ofstream *out) const -> void = 0;

  // Interface to get the type of the operand
  auto GetType() const -> type::Type * { return type_; }
  auto SetType(type::Type *type) -> void { type_ = type; }

  static auto GetTypeInLLVMIR(type::Type *type) -> std::string {
    if (type->GetKind() == type::TypeKind::INT) {
      return "i32";
    }
    if (type->GetKind() == type::TypeKind::FLOAT) {
      return "float";
    }
    if (type->GetKind() == type::TypeKind::BOOL) {
      return "i1";
    }
    if (type->GetKind() == type::TypeKind::VOID) {
      return "void";
    }
    if (type->GetKind() == type::TypeKind::PTR) {
      return "ptr";
    }
    if (type->GetKind() == type::TypeKind::CHAR) {
      return "i8";
    }
    if (type->GetKind() == type::TypeKind::LONG) {
      return "i64";
    }
    // For other types, we can just return the type name as a string
    return type->ToString();
  }

  // Provide some interface to check the information of the operand
  auto IsRegister() const -> bool { return op_type_ == OpType::Register; }
  auto IsLiteral() const -> bool { return op_type_ == OpType::Literal; }
  auto IsMemory() const -> bool { return op_type_ == OpType::Memory; }
  auto IsLabel() const -> bool { return op_type_ == OpType::Label; }
  auto IsGlobal() const -> bool { return op_type_ == OpType::Global; }

  // Provide some interface to check the type of the operand
  auto IsInt32() const -> bool { return type_ != nullptr && type_->GetKind() == type::TypeKind::INT; }
  auto IsFloat() const -> bool { return type_ != nullptr && type_->GetKind() == type::TypeKind::FLOAT; }
  auto IsBool() const -> bool { return type_ != nullptr && type_->GetKind() == type::TypeKind::BOOL; }
  auto IsVoid() const -> bool { return type_ != nullptr && type_->GetKind() == type::TypeKind::VOID; }
  auto IsPtr() const -> bool { return type_ != nullptr && type_->GetKind() == type::TypeKind::PTR; }
  auto IsChar() const -> bool { return type_ != nullptr && type_->GetKind() == type::TypeKind::CHAR; }
  auto IsLong() const -> bool { return type_ != nullptr && type_->GetKind() == type::TypeKind::LONG; }

  // Provide interface to get and set the instruction which generates this op
  auto GetInstruction() const -> inst::Instruction * { return instruction_; }
  auto SetInstruction(inst::Instruction *inst) -> void { instruction_ = inst; }

  virtual ~Operand() = default;

 protected:
  explicit Operand(OpType op_type) : op_type_(op_type) {}

 private:
  const OpType op_type_;
  type::Type *type_ = nullptr;

  /**
   * @brief instruction_ is used to record the instruction which generates
   * this operand,
   * only used in register operand(probably)
   */
  inst::Instruction *instruction_ = nullptr;
};

// Interface for printing the float value in llvm ir format,
// which is 0X followed by the hexadecimal representation of the float value
inline auto FloatToLLVMIRFormat(float value) -> std::string {
  if (value == 0.0F) {
    return "0.000000e+00";
  }

  /**
   * @note:
   * IEEE754 float format: 1 bit sign, 8 bits exponent, 23 bits mantissa
   * for IEEE754 float format: 1 bit sign, 11 bits exponent, 52 bits mantissa
   */
  uint64_t u_value = 0;
  std::memcpy(&u_value, &value, sizeof(float));
  uint64_t sign = (u_value >> 31) & 0x1;
  uint64_t exponent = (u_value >> 23) & 0xFF;
  uint64_t mantissa = u_value & 0x7FFFFF;

  // Get the float format of the float value
  uint64_t float_sign = sign;
  uint64_t float_exponent = exponent - 127 + 1023;  // Exponent bias for float is 1023, while for float is 127
  uint64_t float_mantissa = mantissa << 29;         // Shift the mantissa to the left to fit the float format

  uint64_t float_u_value = (float_sign << 63) | (float_exponent << 52) | float_mantissa;

  std::ostringstream oss;
  oss << "0x" << std::hex << std::uppercase
      << float_u_value;  // Output the float value in hexadecimal format with 0X prefix

  return oss.str();
}

}  // namespace middleend::llvm::utils

#endif