#ifndef TOKEN_FACTORY_H
#define TOKEN_FACTORY_H
#include <sys/types.h>
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
  [[maybe_unused]] auto CreateToken(TokenType type, std::string &lexeme, SourceRange &source_range) -> Token {
    return {type, lexeme, source_range};
  }

  // Factory method overload to create Token
  auto CreateToken(TokenType type, std::string &lexeme, uint32_t line, uint32_t column) -> Token {
    auto range = SourceRange::GetRange(0, line, column, 0, line, column + static_cast<uint32_t>(lexeme.length()));
    return {type, lexeme, range};
  }

  auto CreateToken(TokenType type, const std::string &lexeme, SourceRange &source_range) -> Token {
    return {type, lexeme, source_range};
  }

  auto CreateToken(TokenType type, const std::string &lexeme, uint32_t line, uint32_t column) -> Token {
    auto range = SourceRange::GetRange(0, line, column, 0, line, column + static_cast<uint32_t>(lexeme.length()));
    return {type, lexeme, range};
  }

 private:
  // private constructor for singleton
  TokenFactory() = default;
};

};  // namespace frontend

#endif