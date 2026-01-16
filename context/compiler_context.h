#ifndef CONTEXT_COMPILER_CONTEXT_H
#define CONTEXT_COMPILER_CONTEXT_H

#include "context/ast_context.h"
#include "context/type_context.h"
namespace context {

/**
 * @brief Top context class for the compiler
 * This class will hold global information and states needed during the compilation process.
 */
class CompilerContext {
  // We can create TypeContext at the beginning since it doesn't depend on anything else
  // and will be used throughout the compilation process.
 public:
  CompilerContext() { type_context_ = new TypeContext(); }
  ~CompilerContext() {
    delete type_context_;
    delete ast_context_;
  }
  auto GetTypeContext() -> TypeContext * { return type_context_; }
  auto GetAstContext() -> AstContext * { return ast_context_; }

 private:
  // First we control types pointer here
  TypeContext *type_context_;
  // Then we need to control ast context here
  [[maybe_unused]] AstContext *ast_context_;
};

}  // namespace context

#endif