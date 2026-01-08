#ifndef FRONTEND_FLEX_LEXER_IMPL_H
#define FRONTEND_FLEX_LEXER_IMPL_H

// include the generated header from flex
#include <optional>
#include "lexer.h"
#include "utils/token.h"
#include "utils/token_factory.h"

#if defined(yyFlexLexer)
#undef yyFlexLexer
#endif
#include <FlexLexer.h>

namespace frontend {
// inheriting from both Lexer and yyFlexLexer
class FlexLexerImpl : public Lexer, public yyFlexLexer {
 public:
  explicit FlexLexerImpl(std::istream *in) : yyFlexLexer(in) {}
  auto yylex() -> int override;

  auto GetNextTokenfromImpl() -> Token override {
    [[maybe_unused]] int token_type = yylex();  // Call the generated lexer function
    // We decide that we will generate Token(END) when we reach EOF
    // so we DONT need to handle EOF here

    if (token_slot_.has_value()) {
      Token token = std::move(token_slot_.value());
      // We can handle error tokens here as well
      if (token_type == static_cast<int>(TokenType::UNKNOWN)) {
        // We can handle error reporting here
        // and to be more advanced, we can ignore unknown tokens and continue lexing
        // but for simplicity, we just stop here
        std::string error_msg = "LexerError: Unknown token: " + token.GetLexeme() + " in line " +
                                std::to_string(token.GetSourceRange().GetBegin().GetLine()) + ", column " +
                                std::to_string(token.GetSourceRange().GetBegin().GetColumn());
        LexerError(error_msg.c_str());
      }
      token_slot_.reset();
      return token;
    }

    // normally, we should never reach here, MOST PROBABLELY a BUG in flex.l or in parser
    throw std::runtime_error("FlexLexerImpl: No token generated from yylex()");
  };

  // Then we need to alloca some space to store the yycolumn
 private:
  [[maybe_unused]] int yycolumn_ = 1;
  std::optional<Token> token_slot_;
  auto MakeToken(TokenType type, const char *yytext, int lineno) -> int {
    auto &token_factory = frontend::TokenFactory::GetInstance();
    auto start_column = yycolumn_ - yyleng;
    this->token_slot_.emplace((token_factory.CreateToken(type, yytext, lineno, start_column)));
    return static_cast<int>(type);
  }
};
};  // namespace frontend

#endif