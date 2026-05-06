#ifndef MIDDLEEND_LLVM_INST_BRANCH_H
#define MIDDLEEND_LLVM_INST_BRANCH_H
#include "llvm/utils/operand/labeloperand.h"
#include "middleend/llvm/inst/inst.h"
namespace middleend::llvm::inst {
class BrInst : public Instruction {
 public:
  auto Emit(std::ofstream *out) const -> void override;
  auto Verify(context::CompilerContext *context) const -> bool override;

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

 private:
  explicit BrCondInst() : Instruction(OpKind::CondBr) {}
  static auto Create(context::LLVMIRContext *context, utils::Operand *condition, utils::LabelOperand *true_target,
                     utils::LabelOperand *false_target) -> BrCondInst *;
  friend class utils::factory::InstFactory;

  // Condition operand
  utils::Operand *condition_;
  // True and false target labels
  utils::LabelOperand *true_target_;
  utils::LabelOperand *false_target_;
};

}  // namespace middleend::llvm::inst

#endif