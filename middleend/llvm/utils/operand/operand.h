/**
 * @brief This file defines the operand for llvm ir
 * @note All operand should inherit from this class,
 * and implement the Emit() function to emit the operand as a string,
 * moreover, all of them should only be created via a factory.
 */
#ifndef MIDDLEEND_LLVM_UTILS_OPERAND_OPERAND_H
#define MIDDLEEND_LLVM_UTILS_OPERAND_OPERAND_H
#include <cstdint>
#include <string>
#include "type/type.h"

// Forward declare the operand factory to allow it to create operands
namespace middleend::llvm::utils::factory {
class OperandFactory;
}

namespace middleend::llvm::utils::operand {
enum OpType : uint8_t { Register, Literal, Memory, Label };

class Operand {
 public:
  // Do not allow the operand change its optype after creation
  // Interface to get the operand type
  auto GetOpType() const -> OpType { return op_type_; }

  // Interface to emit the operand
  virtual auto Emit() const -> std::string = 0;

  // Interface to get the type of the operand
  auto GetType() const -> type::Type * { return type_; }
  auto SetType(type::Type *type) -> void { type_ = type; }

 protected:
  explicit Operand(OpType op_type) : op_type_(op_type) {}

 private:
  const OpType op_type_;
  type::Type *type_ = nullptr;
};
}  // namespace middleend::llvm::utils::operand

#endif