#ifndef TOKEN_H
#define TOKEN_H
#include <string>
#include "./location.h"
#include "./token_utils.h"
namespace frontend {

class Token {
 private:
  // Record token type
  [[maybe_unused]] TokenType token_type_;
  // Record lexeme
  std::string lexeme_;
  // Record information about location
  [[maybe_unused]] SourceRange source_range_;

 public:
  // 首先提供一些接口来访问内部成员
  auto GetTokenType() const -> TokenType { return token_type_; }
  auto GetLexeme() const -> const std::string & { return lexeme_; }
  auto GetSourceRange() const -> const SourceRange & { return source_range_; }
};

};  // namespace frontend

#endif