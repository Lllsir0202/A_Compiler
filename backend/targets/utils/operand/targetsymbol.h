#ifndef BACKEND_TARGETS_UTILS_OPERAND_TARGETSYMBOL_H
#define BACKEND_TARGETS_UTILS_OPERAND_TARGETSYMBOL_H

#include "backend/targets/utils/operand/targetoperand.h"
namespace backend::targetir {
class TargetSymbol : public TargetOperand {
 public:
  explicit TargetSymbol(std::string name) : TargetOperand(TargetOperandType::Symbol), name_(std::move(name)) {}

  // Get the name of this symbol
  // The name will record the symbol's name in the assembly code,
  // for example, the name of a global variable or a function's name.
  auto GetName() const -> const std::string & { return name_; }

 protected:
  std::string name_;
};

}  // namespace backend::targetir

#endif