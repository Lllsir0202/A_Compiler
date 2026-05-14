#ifndef MIDDLEEND_LLVM_INST_MEMORY_CONSTINITS_H
#define MIDDLEEND_LLVM_INST_MEMORY_CONSTINITS_H
#include "middleend/llvm/inst/inst.h"
#include "utils/type/type.h"
namespace middleend::llvm::inst {
class ConstInitInst : public Instruction {
 public:
  auto Emit(std::ofstream *out) const -> void override;
  auto Verify(context::CompilerContext *context) const -> bool override;

 private:
  explicit ConstInitInst() : Instruction(OpKind::ConstInit) {}
  static auto Create(context::LLVMIRContext *context, type::Type *type, utils::Operand *value) -> ConstInitInst *;
  friend class utils::factory::InstFactory;
};

}  // namespace middleend::llvm::inst

#endif