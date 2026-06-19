#ifndef BACKEND_TARGETS_UTILS_OPERAND_TARGETLABEL_H
#define BACKEND_TARGETS_UTILS_OPERAND_TARGETLABEL_H

#include "backend/targets/utils/operand/targetoperand.h"
namespace backend::targetir {
class TargetLabel : public TargetOperand {
 public:
  auto Emit(std::ofstream *out) -> void override { *out << name_; }

  // Get the name of this label
  // The name will record the label's name in the assembly code,
  // for example, the name of a basic block or a function's name.
  auto GetName() const -> const std::string & { return name_; }

 private:
  explicit TargetLabel(std::string name) : TargetOperand(TargetOperandType::Label), name_(std::move(name)) {}
  std::string name_;
  friend class OperandFactory;
};

}  // namespace backend::targetir

#endif