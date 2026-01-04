#ifndef TOKEN_H
#define TOKEN_H
#include <iomanip>
#include <string>
#include "./location.h"
#include "./token_utils.h"
namespace frontend {

class TokenFactory;

class Token {
 private:
  // Record token type
  TokenType token_type_;
  // Record lexeme
  std::string lexeme_;
  // Record information about location
  SourceRange source_range_;

  // Private constructor
  Token(TokenType type, std::string &lexeme, SourceRange &source_range)
      : token_type_(type), lexeme_(lexeme), source_range_(source_range) {}
  friend class TokenFactory;

 public:
  // 首先提供一些接口来访问内部成员
  auto GetTokenType() const -> TokenType { return token_type_; }
  auto GetLexeme() const -> const std::string & { return lexeme_; }
  [[maybe_unused]] auto GetSourceRange() const -> const SourceRange & { return source_range_; }
  auto Print(std::ostream &out) -> void {
    out << std::left << std::setw(8) << TokenTypeToString(token_type_) << std::left << std::setw(12) << lexeme_;
  }
};

};  // namespace frontend

#endif