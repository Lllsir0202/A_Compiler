#ifndef TOKEN_H
#define TOKEN_H
#include <iomanip>
#include <string>
#include "./location.h"
#include "./token_utils.h"
namespace frontend {

class Token {
 private:
  // Record token type
  TokenType token_type_;
  // Record lexeme
  std::string lexeme_;
  // Record information about location
  SourceRange source_range_;
  // TODO(loki): add a class to store value

  // Private constructor
  Token(TokenType type, std::string lexeme, SourceRange &source_range)
      : token_type_(type), lexeme_(std::move(lexeme)), source_range_(source_range) {}
  friend class TokenFactory;

 public:
  // provide some getters
  auto GetTokenType() const -> TokenType { return token_type_; }
  auto GetLexeme() const -> const std::string & { return lexeme_; }
  [[maybe_unused]] auto GetSourceRange() const -> const SourceRange & { return source_range_; }
  auto Print(std::ostream &out) -> void {
    out << std::left << std::setw(10) << TokenTypeToString(token_type_) << std::setw(12) << lexeme_;
    source_range_.Print(out);
  }

  // Overloaded Print with width parameter for lexeme to print formally
  auto Print(std::ostream &out, int width) -> void {
    out << std::left << std::setw(10) << TokenTypeToString(token_type_) << std::setw(width) << lexeme_;
    source_range_.Print(out);
  }

  // Overloaded Print with more specific width parameters
  auto Print(std::ostream &out, int type_width, int lexeme_width, int line_width, int column_width) -> void {
    out << std::left << std::setw(type_width) << TokenTypeToString(token_type_) << std::setw(lexeme_width) << lexeme_;
    source_range_.Print(out, line_width, column_width);
  }
};

};  // namespace frontend

#endif