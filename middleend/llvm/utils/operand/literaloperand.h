#ifndef MIDDLEEND_LLVM_UTILS_OPERAND_LITERALOPERAND_H
#define MIDDLEEND_LLVM_UTILS_OPERAND_LITERALOPERAND_H

#include "middleend/llvm/utils/operand/operand.h"
namespace middleend::llvm::utils {
class IntLiteral : public Operand {
 public:
  auto Emit(std::ofstream *out) const -> void override { *out << std::to_string(value_); }

 private:
  explicit IntLiteral(int value) : Operand(OpType::Literal), value_(value) {}

  int value_;
  friend class middleend::llvm::utils::factory::OperandFactory;
};

class FloatLiteral : public Operand {
 public:
  auto Emit(std::ofstream *out) const -> void override { *out << FloatToLLVMIRFormat(value_); }

 private:
  explicit FloatLiteral(float value) : Operand(OpType::Literal), value_(value) {}

  double value_;
  friend class middleend::llvm::utils::factory::OperandFactory;
};

class LongLiteral : public Operand {
 public:
  auto Emit(std::ofstream *out) const -> void override { *out << std::to_string(value_); }

 private:
  explicit LongLiteral(std::int64_t value) : Operand(OpType::Literal), value_(value) {}

  std::int64_t value_;
  friend class middleend::llvm::utils::factory::OperandFactory;
};

class CharLiteral : public Operand {
 public:
  auto Emit(std::ofstream *out) const -> void override { *out << std::to_string(value_); }

 private:
  explicit CharLiteral(char value) : Operand(OpType::Literal), value_(value) {}

  char value_;
  friend class middleend::llvm::utils::factory::OperandFactory;
};

class BoolLiteral : public Operand {
 public:
  auto Emit(std::ofstream *out) const -> void override { *out << (value_ ? "true" : "false"); }

 private:
  explicit BoolLiteral(bool value) : Operand(OpType::Literal), value_(value) {}

  bool value_;
  friend class middleend::llvm::utils::factory::OperandFactory;
};

}  // namespace middleend::llvm::utils

#endif