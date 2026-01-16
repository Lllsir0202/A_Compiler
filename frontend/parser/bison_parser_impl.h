#ifndef FRONTEND_BISON_PARSER_IMPL_H
#define FRONTEND_BISON_PARSER_IMPL_H

#include <cassert>
#include "parser.h"
// The details of bison parser implementation will be introduced in README.md

// Firstly, we need to declare the bison parser class
// Since we use our own token class,
// we need to adapt bison parser to our Parser interface
namespace yy {
class parser;
}
namespace frontend {

/**
 * @name BisonParserImpl class definition
 * @brief since bison only knows that when it needs a token, it will call yylex(),
 * we need to implement a adapter class to adapt our Lexer to bison parser
 * so that when bison parser needs a token, it will call our Lexer to get the token
 * and return it to bison parser
 * This class(BisonParserImpl) will work as a driver/adapter between bison parser and our Lexer
 */
class BisonParserImpl : public Parser {
 public:
  explicit BisonParserImpl(std::unique_ptr<Lexer> lexer) noexcept : Parser(std::move(lexer)) {}
  ~BisonParserImpl() override = default;

  auto Parse(context::CompilerContext &context) -> int override;

  // Since yylex() will be a global function, we can just make GetLexer() public
  auto GetLexer() -> Lexer * { return Parser::GetLexer(); }

  // Set member function to provide access to compiler context
  auto GetContext() -> context::CompilerContext &;

 private:
  context::CompilerContext *compiler_context_;
};

}  // namespace frontend

#ifdef yy_parser_symbol_type
auto yylex(frontend::BisonParserImpl &parser) -> yy::parser::symbol_type;
#endif
#endif