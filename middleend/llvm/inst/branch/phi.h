#ifndef MIDDLEEND_LLVM_INST_BRANCH_PHI_H
#define MIDDLEEND_LLVM_INST_BRANCH_PHI_H

#include "llvm/utils/operand/labeloperand.h"
#include "middleend/llvm/inst/inst.h"
namespace middleend::llvm::inst {
class PhiInst : public Instruction {
 public:
  auto GetIncomingValues() const -> const std::vector<utils::Operand *> & { return incoming_values_; }
  auto GetIncomingBlocks() const -> const std::vector<utils::LabelOperand *> & { return incoming_blocks_; }
  auto AddIncoming(utils::Operand *value, utils::LabelOperand *block) -> void {
    incoming_values_.emplace_back(value);
    incoming_blocks_.emplace_back(block);
  }

  // Virtual overrides
  auto Emit(std::ofstream *out) const -> void override;
  auto Verify(context::CompilerContext *compiler_context) const -> bool override;

 private:
  PhiInst() : Instruction(OpKind::Phi) {}
  static auto Create(context::LLVMIRContext *context, std::vector<utils::Operand *>, std::vector<utils::LabelOperand *>,
                     type::Type *result_type) -> PhiInst *;
  friend class context::LLVMIRContext;

  // Operands of the phi instruction
  std::vector<utils::Operand *> incoming_values_;
  std::vector<utils::LabelOperand *> incoming_blocks_;
};

}  // namespace middleend::llvm::inst

#endif