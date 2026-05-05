#ifndef MIDDLEEND_LLVM_UTILS_OPERAND_LITERALOPERAND_H
#define MIDDLEEND_LLVM_UTILS_OPERAND_LITERALOPERAND_H

#include "middleend/llvm/utils/operand/operand.h"
namespace middleend::llvm::utils::operand {
class IntLiteral : public Operand {
 public:
  auto Emit() const -> std::string override { return std::to_string(value_); }

 private:
  explicit IntLiteral(int value) : Operand(OpType::Literal), value_(value) {}

  int value_;
  friend class middleend::llvm::utils::factory::OperandFactory;
};

class FloatLiteral : public Operand {
 public:
  auto Emit() const -> std::string override { return std::to_string(value_); }

 private:
  explicit FloatLiteral(float value) : Operand(OpType::Literal), value_(value) {}

  float value_;
  friend class middleend::llvm::utils::factory::OperandFactory;
};

}  // namespace middleend::llvm::utils::operand

#endif