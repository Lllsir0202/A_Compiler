#ifndef MIDDLEEND_LLVM_VERIFER_VERIFIER_H
#define MIDDLEEND_LLVM_VERIFER_VERIFIER_H

#include "llvm/utils/struct/module.h"
namespace middleend::llvm::verifer {
class LLVMVerifer {
 public:
  LLVMVerifer(llvm::utils::Module *module, context::CompilerContext *compiler_context)
      : module_(module), compiler_context_(compiler_context) {}
  auto Verify() -> bool;

 private:
  llvm::utils::Module *module_;
  context::CompilerContext *compiler_context_;
};

}  // namespace middleend::llvm::verifer

#endif