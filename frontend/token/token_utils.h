#ifndef TOKEN_UTILS_H
#define TOKEN_UTILS_H

#include <cstdint>
#include <string>

namespace frontend {

// Define token types
enum class TokenType : uint8_t {
#define TOKEN(name) name,
#include "token.def"
#undef TOKEN
};

// Define Helper used to transform TokenType to string
[[maybe_unused]] inline auto TokenTypeToString(TokenType type) -> std::string {
  switch (type) {
#define TOKEN(name)     \
  case TokenType::name: \
    return #name;
#include "token.def"
#undef TOKEN
    default:
      return "UNKNOWN_TYPE";
  }
}

// Define Helper used to check token type
[[maybe_unused]] inline auto IsOperator(TokenType type) -> bool {
  return type > TokenType::OpBegin && type < TokenType::OpEnd;
}

[[maybe_unused]] inline auto IsIntConst(TokenType type) -> bool { return type == TokenType::INTCONST; }

[[maybe_unused]] inline auto IsFloatConst(TokenType type) -> bool { return type == TokenType::FLOATCONST; }

[[maybe_unused]] inline auto IsKeyWord(TokenType type) -> bool {
  return type > TokenType::KWBegin && type < TokenType::KWEnd;
}

};  // namespace frontend
#endif