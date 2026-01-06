#ifndef FRONTEND_LEXER_H
#define FRONTEND_LEXER_H
#include "../utils/token.h"

namespace frontend {
// Virtual Lexer class : adapter to all kinds of lexers
class Lexer {
 public:
  virtual ~Lexer() = default;
  // Get next token from the input stream
  virtual auto GetNextToken() -> Token = 0;
};

};  // namespace frontend

#endif