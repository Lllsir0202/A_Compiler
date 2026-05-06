#ifndef MIDDLEEND_LLVM_INST_BRANCH_RET_H
#define MIDDLEEND_LLVM_INST_BRANCH_RET_H

#include "middleend/llvm/inst/inst.h"
namespace middleend::llvm::inst {
class RetInst : public Instruction {
 public:
  auto Emit(std::ofstream *out) const -> void override;
  auto Verify(context::CompilerContext *context) const -> bool override;

 private:
  explicit RetInst() : Instruction(OpKind::Ret) {}
  static auto Create(context::LLVMIRContext *context, utils::Operand *ret_value) -> RetInst *;
  friend class utils::factory::InstFactory;

  // Return value operand, can be nullptr for void return
  utils::Operand *ret_value_;
};

}  // namespace middleend::llvm::inst

#endif