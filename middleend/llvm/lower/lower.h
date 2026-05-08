#ifndef MIDDLEEND_LLVM_LOWER_LOWER_H
#define MIDDLEEND_LLVM_LOWER_LOWER_H

namespace context {
class CompilerContext;
}

namespace middleend::llvm::lower {
class LLVMLower {
 public:
  explicit LLVMLower(context::CompilerContext *compiler_context);
  ~LLVMLower() = default;
  auto Lower() -> int;

 private:
  context::CompilerContext *compiler_context_;
};

}  // namespace middleend::llvm::lower

#endif