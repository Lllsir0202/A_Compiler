#ifndef BACKEND_MIR_UTILS_OPERAND_LITERALOPERAND_H
#define BACKEND_MIR_UTILS_OPERAND_LITERALOPERAND_H

#include "backend/mir/utils/operand/operand.h"
namespace backend::utils {
// Since the literal value's range varies among different architectures,
// so we can just record the value and deal with them separately in the lowering phase.
class IntLiteral : public Operand {
 public:
  auto GetValue() const -> int { return value_; }

 private:
  explicit IntLiteral(int value) : Operand(OpType::Literal), value_(value) {}
  int value_;
  friend class factory::Operandfactory;
};

class FloatLiteral : public Operand {
 public:
  auto GetValue() const -> float { return value_; }

 private:
  explicit FloatLiteral(float value) : Operand(OpType::Literal), value_(value) {}
  float value_;
  friend class factory::Operandfactory;
};

class BoolLiteral : public Operand {
 public:
  auto GetValue() const -> bool { return value_; }

 private:
  explicit BoolLiteral(bool value) : Operand(OpType::Literal), value_(value) {}
  bool value_;
  friend class factory::Operandfactory;
};

class LongLiteral : public Operand {
 public:
  auto GetValue() const -> std::int64_t { return value_; }

 private:
  explicit LongLiteral(std::int64_t value) : Operand(OpType::Literal), value_(value) {}
  std::int64_t value_;
  friend class factory::Operandfactory;
};

class CharLiteral : public Operand {
 public:
  auto GetValue() const -> char { return value_; }

 private:
  explicit CharLiteral(char value) : Operand(OpType::Literal), value_(value) {}
  char value_;
  friend class factory::Operandfactory;
};

}  // namespace backend::utils

#endif