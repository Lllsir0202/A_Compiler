#ifndef FRONTEND_PARSER_H
#define FRONTEND_PARSER_H

#include <memory>
#include "../frontend/lexer/lexer.h"
#include "context/compiler_context.h"

namespace frontend {
// Virtual Parser class : adapter to all kinds of parsers
class Parser {
 public:
  explicit Parser(std::unique_ptr<Lexer> lexer, context::CompilerContext& compiler_context) noexcept : lexer_(std::move(lexer)), compiler_context_(compiler_context) {}
  virtual ~Parser() = default;
  // Parse the input tokens and build AST
  virtual auto Parse() -> void = 0;

 protected:
  // Provide access to the lexer for derived classes
  auto GetLexer() -> Lexer * { return lexer_.get(); }
  auto GetCompilerContext() -> context::CompilerContext& { return compiler_context_; }

 private:
  std::unique_ptr<Lexer> lexer_;
  context::CompilerContext& compiler_context_;
};
}  // namespace frontend

#endif