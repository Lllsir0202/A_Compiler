#ifndef MIDDLEEND_LLVM_INST_FUNC_CALL_H
#define MIDDLEEND_LLVM_INST_FUNC_CALL_H
#include "middleend/llvm/inst/inst.h"
#include "utils/type/type.h"

namespace middleend::llvm::inst {
class CallInst : public Instruction {
 public:
  auto Emit(std::ofstream *out) const -> void override;
  auto Verify(context::CompilerContext *context) const -> bool override;

 private:
  explicit CallInst() : Instruction(OpKind::Call) {}
  static auto Create(context::LLVMIRContext *context, utils::Operand *callee, const std::vector<utils::Operand *> &args,
                     type::Type *return_type) -> CallInst *;
  friend class utils::factory::InstFactory;

  // String name of the callee function
  std::string callee_;
  // Argument operands
  std::vector<utils::Operand *> args_;
  // Return type of the call instruction
  type::Type *return_type_;
};
}  // namespace middleend::llvm::inst

#endif