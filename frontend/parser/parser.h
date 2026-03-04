#ifndef FRONTEND_PARSER_H
#define FRONTEND_PARSER_H

#include <cassert>
#include <memory>
#include "../frontend/lexer/lexer.h"
#include "context/compiler_context.h"

namespace frontend {
// Virtual Parser class : adapter to all kinds of parsers
class Parser {
 public:
  explicit Parser(std::unique_ptr<Lexer> lexer) noexcept : lexer_(std::move(lexer)) {}
  virtual ~Parser() = default;
  // Parse the input tokens and build AST
  // Since we don't know the details of each parser,
  // we declare this method as pure virtual
  virtual auto Parse(context::CompilerContext &context) -> int = 0;

  /**
   * @brief Report error function for parser
   * since I suppose different parser implementations DO NOT have their own error reporting mechanisms,
   * so we provide a common error reporting function here
   */
  auto ReportError(SourceRange source_range, const std::string &msg) -> void {
    assert(compiler_context_ != nullptr && "In parser error reproting, compiler context is not set");
    compiler_context_->ReportError(source_range, msg);
  }

 protected:
  // Provide access to the lexer for derived classes
  auto GetLexer() -> Lexer * { return lexer_.get(); }
  /**
   * @brief We only need to hold a pointer to compiler context while in parsing
   * since in ReportError we will always hold this pointer, so we can imply it in context
   */
  context::CompilerContext *compiler_context_;

 private:
  std::unique_ptr<Lexer> lexer_;
};
}  // namespace frontend

#endif