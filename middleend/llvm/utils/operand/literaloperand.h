#ifndef MIDDLEEND_LLVM_UTILS_OPERAND_LITERALOPERAND_H
#define MIDDLEEND_LLVM_UTILS_OPERAND_LITERALOPERAND_H

#include "middleend/llvm/utils/operand/operand.h"
namespace middleend::llvm::utils {
/**
 * @note Only allow access to the value once the type is verified.
 */
class IntLiteral : public Operand {
 public:
  auto Emit(std::ofstream *out) const -> void override { *out << std::to_string(value_); }
  auto GetValue() const -> int { return value_; }
  auto Isconstant() const -> bool override { return true; }

 private:
  explicit IntLiteral(int value) : Operand(OpType::Literal), value_(value) {}

  int value_;
  friend class middleend::llvm::utils::factory::OperandFactory;
};

class FloatLiteral : public Operand {
 public:
  auto Emit(std::ofstream *out) const -> void override { *out << FloatToLLVMIRFormat(value_); }
  auto GetValue() const -> float { return value_; }
  auto Isconstant() const -> bool override { return true; }

 private:
  explicit FloatLiteral(float value) : Operand(OpType::Literal), value_(value) {}

  float value_;
  friend class middleend::llvm::utils::factory::OperandFactory;
};

class LongLiteral : public Operand {
 public:
  auto Emit(std::ofstream *out) const -> void override { *out << std::to_string(value_); }
  auto GetValue() const -> std::int64_t { return value_; }
  auto Isconstant() const -> bool override { return true; }

 private:
  explicit LongLiteral(std::int64_t value) : Operand(OpType::Literal), value_(value) {}

  std::int64_t value_;
  friend class middleend::llvm::utils::factory::OperandFactory;
};

class CharLiteral : public Operand {
 public:
  auto Emit(std::ofstream *out) const -> void override { *out << std::to_string(value_); }
  auto GetValue() const -> char { return value_; }
  auto Isconstant() const -> bool override { return true; }

 private:
  explicit CharLiteral(char value) : Operand(OpType::Literal), value_(value) {}

  char value_;
  friend class middleend::llvm::utils::factory::OperandFactory;
};

class BoolLiteral : public Operand {
 public:
  auto Emit(std::ofstream *out) const -> void override { *out << (value_ ? "true" : "false"); }
  auto GetValue() const -> bool { return value_; }
  auto Isconstant() const -> bool override { return true; }

 private:
  explicit BoolLiteral(bool value) : Operand(OpType::Literal), value_(value) {}

  bool value_;
  friend class middleend::llvm::utils::factory::OperandFactory;
};

}  // namespace middleend::llvm::utils

#endif