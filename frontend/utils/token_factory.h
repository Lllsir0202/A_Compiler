#ifndef TOKEN_FACTORY_H
#define TOKEN_FACTORY_H
#include <string>
#include "./location.h"
#include "./token.h"
#include "./token_utils.h"

namespace frontend {

class TokenFactory {
 public:
  // Get global singleton instance
  static auto GetInstance() -> TokenFactory & {
    static TokenFactory instance;
    return instance;
  }

  // Forbid copy and assignment
  TokenFactory(const TokenFactory &) = delete;
  auto operator=(const TokenFactory &) -> TokenFactory & = delete;

  // Factory method to create Token instances
  auto CreateToken(TokenType type, std::string &lexeme, SourceRange &source_range) -> Token {
    return {type, lexeme, source_range};
  }

 private:
  // private constructor for singleton
  TokenFactory() = default;
};

};  // namespace frontend

#endif