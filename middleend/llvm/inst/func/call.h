#ifndef MIDDLEEND_LLVM_INST_FUNC_CALL_H
#define MIDDLEEND_LLVM_INST_FUNC_CALL_H
#include "middleend/llvm/inst/inst.h"
#include "utils/type/type.h"

namespace middleend::llvm::inst {
class CallInst : public Instruction {
 public:
  auto Emit(std::ofstream *out) const -> void override;
  auto Verify(context::CompilerContext *context) const -> bool override;

  auto GetCallee() const -> const std::string & { return callee_; }
  auto GetArgs() const -> const std::vector<utils::Operand *> & { return operands_; }
  auto GetReturnType() const -> type::Type * { return return_type_; }

 private:
  explicit CallInst() : Instruction(OpKind::Call) {}
  static auto Create(context::LLVMIRContext *context, std::string callee, const std::vector<utils::Operand *> &args,
                     type::Type *return_type) -> CallInst *;
  friend class utils::factory::InstFactory;

  // String name of the callee function
  std::string callee_;

  // Return type of the call instruction
  type::Type *return_type_;
};
}  // namespace middleend::llvm::inst

#endif