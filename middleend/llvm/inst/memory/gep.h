#ifndef MIDDLEEND_LLVM_INST_MEMORY_GEP_H
#define MIDDLEEND_LLVM_INST_MEMORY_GEP_H
#include "middleend/llvm/inst/inst.h"
namespace middleend::llvm::inst {
class GetElementPtrInst : public Instruction {
 public:
  // Interfaces to get gep information

  // Override the Emit and Verify functions
  auto Emit(std::ofstream *out) const -> void override;
  auto Verify(context::CompilerContext *context) const -> bool override;

 private:
  explicit GetElementPtrInst() : Instruction(OpKind::GetElementPtr) {}

  /**
   * @brief GEP instruction is like:
   * %ptr = getelementptr <type>, <type>* <base>, i32 <index1>, i32 <index2>, ...
   */
};

}  // namespace middleend::llvm::inst

#endif