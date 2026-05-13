#ifndef MIDDLEEND_LLVM_INST_PSEUDO_PSEUDO_H
#define MIDDLEEND_LLVM_INST_PSEUDO_PSEUDO_H
#include "middleend/llvm/inst/inst.h"
#include "type/type.h"
namespace middleend::llvm::inst {
class CopyInst : public Instruction {
 public:
  auto Emit(std::ofstream *out) const -> void override;
  auto Verify(context::CompilerContext *context) const -> bool override;

  auto GetSrc() const -> utils::Operand * { return operands_[0]; }

 private:
  explicit CopyInst() : Instruction(OpKind::Copy) {}
  static auto Create(context::LLVMIRContext *context, type::Type *result_type, utils::Operand *src) -> CopyInst *;
  friend class utils::factory::InstFactory;
};

}  // namespace middleend::llvm::inst

#endif