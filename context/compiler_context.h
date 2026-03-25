#ifndef CONTEXT_COMPILER_CONTEXT_H
#define CONTEXT_COMPILER_CONTEXT_H

#include <fstream>
#include "ast/ast_base.h"
#include "context/ast_context.h"
#include "context/symboltable_context.h"
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
    symboltable_context_ = new SymbolTableContext();
    InitializeLibFunc();
  }
  ~CompilerContext() {
    delete type_context_;
    delete ast_context_;
    delete symboltable_context_;
  }
  auto GetTypeContext() -> TypeContext * { return type_context_; }
  auto GetAstContext() -> AstContext * { return ast_context_; }
  auto GetSymbolTableContext() -> SymbolTableContext * { return symboltable_context_; }
  auto ReportError(frontend::SourceRange source_range, const std::string &msg) -> void;
  auto ReportError(frontend::ast::ASTNode *node, const std::string &msg) -> void;
  auto ReportError(const std::string &msg) -> void;

 private:
  // First we control types pointer here
  TypeContext *type_context_;
  // Then we need to control ast context here
  AstContext *ast_context_;
  // We need to control symbol table context
  SymbolTableContext *symboltable_context_;

  // Using this compiler context to control ALL resources in the compiler

  /**
   * @brief In the future, we will add input/output stream management here
   * and we will contruct lexer and parser using these streams
   */
  [[maybe_unused]] std::ifstream *in_;
  std::ofstream *out_;

  /**
   * @brief Register library functions in the Sysy
   */
  auto InitializeLibFunc() -> void;
};

}  // namespace context

#endif