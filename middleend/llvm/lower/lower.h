#ifndef MIDDLEEND_LLVM_LOWER_LOWER_H
#define MIDDLEEND_LLVM_LOWER_LOWER_H

#include "llvm/lower/lowerhelper.h"
namespace context {
class CompilerContext;
}

namespace middleend::llvm::lower {
class LLVMLower {
 public:
  explicit LLVMLower(context::CompilerContext *compiler_context) : compiler_context_(compiler_context) {
    lower_helper_ = new LowerHelper(compiler_context);
  }
  ~LLVMLower() { delete lower_helper_; }
  auto Lower() -> int;

 private:
  context::CompilerContext *compiler_context_;
  LowerHelper *lower_helper_;

  // Register all libraray functions
  auto RegisterLibraryFunctions() -> void;
};

}  // namespace middleend::llvm::lower

#endif