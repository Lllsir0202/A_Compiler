#include "lexer_stage.h"

namespace stages {

/**
 * @brief Collect all tokens from the lexer until EOF
 * We decide to collect all tokens first, then print them out
 * since it is easier to manage the output format this way
 */
auto LexerStage::Collect() -> void {
  while (true) {
    frontend::Token token = lexer_->GetNextToken();
    tokens_.push_back(std::move(token));
    if (tokens_.back().GetTokenType() == frontend::TokenType::END) {
      break;
    }
  }
  // Then calculate the max lexeme width for pretty print
  for (const auto &token : tokens_) {
    size_t tokentype_width = TokenTypeToString(token.GetTokenType()).length();
    size_t lexeme_width = token.GetLexeme().length();
    if (tokentype_width > max_tokentype_width_) {
      max_tokentype_width_ = tokentype_width;
    }
    if (lexeme_width > max_lexeme_width_) {
      max_lexeme_width_ = lexeme_width;
    }
    // Also calculate max line and column for location print
    const auto &source_range = token.GetSourceRange();
    const auto start_location = source_range.GetBegin();
    std::string line_str = std::to_string(start_location.GetLine());
    std::string column_str = std::to_string(start_location.GetColumn());
    if (line_str.length() > max_line_) {
      max_line_ = line_str.length();
    }
    if (column_str.length() > max_column_) {
      max_column_ = column_str.length();
    }
  }

  // Add some padding
  max_tokentype_width_ += 2;
  max_lexeme_width_ += 2;
  max_line_ += 2;
  max_column_ += 2;
}

/**
 * @brief Print all tokens from the lexer to the output file
 * @attention After calling Stage we will destroy the whole compiler
 */
auto LexerStage::Print() -> void {
  // First print the header
  output_ << std::left << std::setw(max_tokentype_width_) << "TokenType" << std::setw(max_lexeme_width_) << "Lexeme"
          << std::setw(max_line_) << "Line" << std::setw(max_column_) << "Column" << "\n\n";
  // Then print all tokens
  while (!tokens_.empty()) {
    auto &token = tokens_.front();
    if (token.GetTokenType() == frontend::TokenType::END) {
      tokens_.pop_front();
      break;
    }
    token.Print(output_, max_tokentype_width_, max_lexeme_width_, max_line_, max_column_);
    tokens_.pop_front();
  }
}

}  // namespace stages