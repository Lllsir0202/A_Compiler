#ifndef MIDDLEEND_LLVM_INST_BRANCH_H
#define MIDDLEEND_LLVM_INST_BRANCH_H
#include "llvm/utils/operand/labeloperand.h"
#include "middleend/llvm/inst/inst.h"
namespace middleend::llvm::inst {
class BrInst : public Instruction {
 public:
  auto Emit(std::ofstream *out) const -> void override;
  auto Verify(context::CompilerContext *context) const -> bool override;

  auto GetTargetBlockID() const -> size_t { return target_->GetID(); }
  auto GetTarget() const -> utils::LabelOperand * { return target_; }
  auto SetTarget(utils::LabelOperand *target) -> void { target_ = target; }

 private:
  explicit BrInst() : Instruction(OpKind::Br) {}
  static auto Create(context::LLVMIRContext *context, utils::LabelOperand *target) -> BrInst *;
  friend class utils::factory::InstFactory;

  // Target label
  utils::LabelOperand *target_;
};

class BrCondInst : public Instruction {
 public:
  auto Emit(std::ofstream *out) const -> void override;
  auto Verify(context::CompilerContext *context) const -> bool override;
  auto GetCondition() const -> utils::Operand * { return operands_[0]; }
  auto GetTrueTargetBlockID() const -> size_t { return true_target_->GetID(); }
  auto GetFalseTargetBlockID() const -> size_t { return false_target_->GetID(); }
  auto GetTrueTarget() const -> utils::LabelOperand * { return true_target_; }
  auto SetTrueTarget(utils::LabelOperand *true_target) -> void { true_target_ = true_target; }
  auto GetFalseTarget() const -> utils::LabelOperand * { return false_target_; }
  auto SetFalseTarget(utils::LabelOperand *false_target) -> void { false_target_ = false_target; }

 private:
  explicit BrCondInst() : Instruction(OpKind::CondBr) {}
  static auto Create(context::LLVMIRContext *context, utils::Operand *condition, utils::LabelOperand *true_target,
                     utils::LabelOperand *false_target) -> BrCondInst *;
  friend class utils::factory::InstFactory;
  // True and false target labels
  utils::LabelOperand *true_target_;
  utils::LabelOperand *false_target_;
};

}  // namespace middleend::llvm::inst

#endif
