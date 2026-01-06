#ifndef FRONTEND_PARSER_H
#define FRONTEND_PARSER_H

#include <memory>
#include "../frontend/lexer/lexer.h"

namespace frontend {
// Virtual Parser class : adapter to all kinds of parsers
class Parser {
 public:
  explicit Parser(std::unique_ptr<Lexer> lexer) noexcept : lexer_(std::move(lexer)) {}
  virtual ~Parser() = default;
  // Parse the input tokens and build AST
  virtual auto Parse() -> void = 0;

 protected:
  // Provide access to the lexer for derived classes
  auto GetLexer() -> Lexer * { return lexer_.get(); }

 private:
  std::unique_ptr<Lexer> lexer_;
};
}  // namespace frontend

#endif