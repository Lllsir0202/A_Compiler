#ifndef BACKEND_TARGETS_UTILS_INST_TARGETINST_H
#define BACKEND_TARGETS_UTILS_INST_TARGETINST_H
#include <fstream>
#include <vector>

namespace context {
class BackendContext;
}

namespace backend::targetir {
class TargetOperand;
class TargetBlock;

class TargetInst {
 public:
  virtual auto Emit(std::ofstream *out) -> void = 0;
  virtual ~TargetInst() = default;

  // Common interface for adding operands and setting result
  auto AddOperand(TargetOperand *operand) -> void { operands_.emplace_back(operand); }
  auto SetResult(TargetOperand *result) -> void { result_ = result; }
  auto GetOperands() -> const std::vector<TargetOperand *> & { return operands_; }
  auto GetResult() -> TargetOperand * { return result_; }

  // Replace an operand with another operand, used for register allocation
  // We will replace all virtual register operands with physical register operands after register allocation
  auto ReplaceOperand(TargetOperand *old_operand, TargetOperand *new_operand) -> void;
  auto ReplaceResult(TargetOperand *new_result) -> void { result_ = new_result; }

 protected:
  TargetInst() = default;
  // Operands of this instruction
  std::vector<TargetOperand *> operands_;
  TargetOperand *result_;
};

}  // namespace backend::targetir

#endif