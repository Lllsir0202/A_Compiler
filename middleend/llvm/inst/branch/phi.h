#ifndef MIDDLEEND_LLVM_INST_BRANCH_PHI_H
#define MIDDLEEND_LLVM_INST_BRANCH_PHI_H

#include <stdexcept>
#include "llvm/utils/operand/labeloperand.h"
#include "middleend/llvm/inst/inst.h"
namespace middleend::llvm::inst {
class PhiInst : public Instruction {
 public:
  auto GetIncomingValues() const -> const std::vector<utils::Operand *> & { return operands_; }
  auto GetIncomingBlocks() const -> const std::vector<utils::LabelOperand *> & { return incoming_blocks_; }
  auto AddIncoming(utils::Operand *value, utils::LabelOperand *block) -> void {
    operands_.emplace_back(value);
    incoming_blocks_.emplace_back(block);
  }
  auto UpdateIncoming(size_t index, utils::Operand *value, utils::LabelOperand *block) -> void {
    if (index >= operands_.size()) {
      throw std::out_of_range("Index out of range in UpdateIncoming");
    }
    operands_[index] = value;
    incoming_blocks_[index] = block;
  }

  auto GetIncomingSize() const -> size_t { return operands_.size(); }

  // Virtual overrides
  auto Emit(std::ofstream *out) const -> void override;
  auto Verify(context::CompilerContext *compiler_context) const -> bool override;

 private:
  PhiInst() : Instruction(OpKind::Phi) {}
  static auto Create(context::LLVMIRContext *context, std::vector<utils::Operand *>, std::vector<utils::LabelOperand *>,
                     type::Type *result_type) -> PhiInst *;
  friend class utils::factory::InstFactory;

  // Operands of the phi instruction
  std::vector<utils::LabelOperand *> incoming_blocks_;
};

}  // namespace middleend::llvm::inst

#endif
