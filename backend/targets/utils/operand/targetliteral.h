#ifndef BACKEND_TARGETS_UTILS_OPERAND_TARGETLITERAL_H
#define BACKEND_TARGETS_UTILS_OPERAND_TARGETLITERAL_H
#include "backend/targets/utils/operand/targetoperand.h"
namespace backend::targetir {
class TargetContext;
class TargetIntLiteral : public TargetOperand {
 public:
  auto Emit(std::ofstream *out) -> void override { *out << value_; }
  auto GetValue() -> int { return value_; }

 private:
  explicit TargetIntLiteral(int value) : TargetOperand(TargetOperandType::Immediate), value_(value) {}
  int value_;
  friend class TargetContext;
};

class TargetFloatLiteral : public TargetOperand {
 public:
  auto Emit(std::ofstream *out) -> void override { *out << value_; }
  auto GetValue() -> float { return value_; }

 private:
  explicit TargetFloatLiteral(float value) : TargetOperand(TargetOperandType::Immediate), value_(value) {}
  float value_;
};

}  // namespace backend::targetir

#endif