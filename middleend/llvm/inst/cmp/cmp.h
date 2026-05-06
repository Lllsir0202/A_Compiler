#ifndef MIDDLEEND_LLVM_INST_CMP_H
#define MIDDLEEND_LLVM_INST_CMP_H
#include "middleend/llvm/inst/inst.h"
namespace middleend::llvm::inst {
class ICmpInst : public Instruction {
 public:
  auto Emit(std::ofstream *out) const -> void override;
  auto Verify(context::CompilerContext *context) const -> bool override;

 private:
  explicit ICmpInst(OpKind op_kind) : Instruction(op_kind) {}
  friend class utils::factory::InstFactory;
};

}  // namespace middleend::llvm::inst

#endif