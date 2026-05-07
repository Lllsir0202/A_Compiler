#ifndef MIDDLEEND_LLVM_INST_MEMORY_LOAD_H
#define MIDDLEEND_LLVM_INST_MEMORY_LOAD_H

#include "middleend/llvm/inst/inst.h"
#include "utils/type/type.h"
namespace middleend::llvm::inst {
class LoadInst : public Instruction {
 public:
  // Override the Emit and Verify functions
  auto Emit(std::ofstream *out) const -> void override;
  auto Verify(context::CompilerContext *context) const -> bool override;

  auto GetPtr() const -> utils::Operand * { return operands_[0]; }

 private:
  explicit LoadInst() : Instruction(OpKind::Load) {}
  static auto Create(context::LLVMIRContext *context, type::Type *result_type, utils::Operand *ptr) -> LoadInst *;
  friend class utils::factory::InstFactory;
};

}  // namespace middleend::llvm::inst

#endif