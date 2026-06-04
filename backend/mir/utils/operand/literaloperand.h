#ifndef BACKEND_MIR_UTILS_OPERAND_LITERALOPERAND_H
#define BACKEND_MIR_UTILS_OPERAND_LITERALOPERAND_H

#include "backend/mir/utils/operand/operand.h"
namespace backend::utils {
// Since the literal value's range varies among different architectures,
// so we can just record the value and deal with them separately in the lowering phase.
class IntLiteral : public Operand {
 public:
  auto Emit(std::ofstream *out) const -> void override {
    // The int literal will be emitted as the literal value
    *out << value_;
  }
  auto GetValue() const -> int { return value_; }

 private:
  explicit IntLiteral(int value) : Operand(OpType::Literal), value_(value) {}
  int value_;
  friend class factory::OperandFactory;
};

class FloatLiteral : public Operand {
 public:
  auto Emit(std::ofstream *out) const -> void override {
    // The float literal will be emitted as the literal value
    *out << value_;
  }
  auto GetValue() const -> float { return value_; }

 private:
  explicit FloatLiteral(float value) : Operand(OpType::Literal), value_(value) {}
  float value_;
  friend class factory::OperandFactory;
};

class BoolLiteral : public Operand {
 public:
  auto Emit(std::ofstream *out) const -> void override {
    // The bool literal will be emitted as the literal value
    *out << (value_ ? "true" : "false");
  }
  auto GetValue() const -> bool { return value_; }

 private:
  explicit BoolLiteral(bool value) : Operand(OpType::Literal), value_(value) {}
  bool value_;
  friend class factory::OperandFactory;
};

class LongLiteral : public Operand {
 public:
  auto Emit(std::ofstream *out) const -> void override {
    // The long literal will be emitted as the literal value
    *out << value_;
  }
  auto GetValue() const -> std::int64_t { return value_; }

 private:
  explicit LongLiteral(std::int64_t value) : Operand(OpType::Literal), value_(value) {}
  std::int64_t value_;
  friend class factory::OperandFactory;
};

class CharLiteral : public Operand {
 public:
  auto Emit(std::ofstream *out) const -> void override {
    // The char literal will be emitted as the literal value
    *out << value_;
  }
  auto GetValue() const -> char { return value_; }

 private:
  explicit CharLiteral(char value) : Operand(OpType::Literal), value_(value) {}
  char value_;
  friend class factory::OperandFactory;
};

}  // namespace backend::utils

#endif