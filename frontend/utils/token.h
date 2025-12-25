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
};

};  // namespace frontend

#endif