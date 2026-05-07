#ifndef MIDDLEEND_LLVM_INST_MEMORY_STORE_H
#define MIDDLEEND_LLVM_INST_MEMORY_STORE_H

#include "middleend/llvm/inst/inst.h"
#include "utils/type/type.h"
namespace middleend::llvm::inst {
class StoreInst : public Instruction {
 public:
  // Override the Emit and Verify functions
  auto Emit(std::ofstream *out) const -> void override;
  auto Verify(context::CompilerContext *context) const -> bool override;

  auto GetValue() const -> utils::Operand * { return operands_[0]; }
  auto GetPtr() const -> utils::Operand * { return operands_[1]; }

 private:
  explicit StoreInst() : Instruction(OpKind::Store) {}
  static auto Create(context::LLVMIRContext *context, utils::Operand *value, utils::Operand *ptr) -> StoreInst *;
  friend class utils::factory::InstFactory;
};

}  // namespace middleend::llvm::inst

#endif