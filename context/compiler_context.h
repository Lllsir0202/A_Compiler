#ifndef CONTEXT_COMPILER_CONTEXT_H
#define CONTEXT_COMPILER_CONTEXT_H

#include <fstream>
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
  CompilerContext(std::ifstream *in, std::ofstream *out) : in_(in), out_(out) {
    type_context_ = new TypeContext();
    ast_context_ = new AstContext();
  }
  ~CompilerContext() {
    delete type_context_;
    delete ast_context_;
  }
  auto GetTypeContext() -> TypeContext * { return type_context_; }
  auto GetAstContext() -> AstContext * { return ast_context_; }
  auto ReportError(frontend::SourceRange source_range, const std::string &msg) -> void;

 private:
  // First we control types pointer here
  TypeContext *type_context_;
  // Then we need to control ast context here
  AstContext *ast_context_;

  // Using this compiler context to control ALL resources in the compiler

  /**
   * @brief In the future, we will add input/output stream management here
   * and we will contruct lexer and parser using these streams
   */
  std::ifstream *in_;
  std::ofstream *out_;
};

}  // namespace context

#endif