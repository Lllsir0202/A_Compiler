#ifndef BACKEND_TARGETS_UTILS_INST_TARGETINST_H
#define BACKEND_TARGETS_UTILS_INST_TARGETINST_H
#include <fstream>
#include <vector>
namespace backend::targetir {
class TargetOperand;
class TargetBlock;

class TargetInst {
 public:
  virtual ~TargetInst() = default;
  virtual auto Emit(std::ofstream *out) -> void = 0;

  // Common interface for adding operands and setting result
  auto AddOperand(TargetOperand *operand) -> void { operands_.emplace_back(operand); }
  auto SetResult(TargetOperand *result) -> void { result_ = result; }
  // Replace the whole operand list, used by machine peephole passes that rewrite
  // an instruction's operands (e.g. folding a constant into an immediate form).
  auto SetOperands(std::vector<TargetOperand *> operands) -> void { operands_ = std::move(operands); }
  auto GetOperands() -> const std::vector<TargetOperand *> & { return operands_; }
  auto GetResult() -> TargetOperand * { return result_; }

  // Replace an operand with another operand, used for register allocation
  // We will replace all virtual register operands with physical register operands after register allocation
  auto ReplaceOperand(TargetOperand *old_operand, TargetOperand *new_operand) -> void;
  auto ReplaceResult(TargetOperand *new_result) -> void { result_ = new_result; }

 protected:
  // Operands of this instruction
  std::vector<TargetOperand *> operands_;
  // nullptr for instructions without a result (branches, stores, calls, ret).
  TargetOperand *result_ = nullptr;
};

}  // namespace backend::targetir

#endif