#ifndef FRONTEND_LEXER_H
#define FRONTEND_LEXER_H
#include <deque>
#include "../utils/token.h"

namespace frontend {
class TokenFactory;
// Virtual Lexer class : adapter to all kinds of lexers
class Lexer {
  // Here we provide interface for two types of lexers:
  // 1. stream lexer
  // 2. buffer lexer
 public:
  virtual ~Lexer() = default;
  virtual auto GetNextTokenfromImpl() -> Token = 0;
  // Get next token from the input stream
  auto GetNextToken() -> Token {
    if (token_buffer_.empty()) {
      return GetNextTokenfromImpl();
    }
    Token token = token_buffer_.front();
    token_buffer_.pop_front();
    return token;
  }
  // Add next token back to the stream
  auto AddTokenBack(const Token &token) -> void { token_buffer_.push_front(token); }
  // Peek the next token without consuming it
  auto PeekToken(int offset) -> Token {
    if (offset >= static_cast<int>(token_buffer_.size())) {
      // Need to fetch more tokens
      for (int i = token_buffer_.size(); i <= offset; i++) {
        token_buffer_.push_back(GetNextTokenfromImpl());
      }
    }
    return token_buffer_[offset];
  }
  // Then we need some methods to manage the lexer state as well as the source manager which is used to manage Tokens
 private:
  std::deque<Token> token_buffer_;
};

};  // namespace frontend

#endif