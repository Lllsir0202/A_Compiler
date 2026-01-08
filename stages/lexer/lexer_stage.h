#ifndef LEXER_STAGE_H
#define LEXER_STAGE_H
#include <cstddef>
#include <fstream>
#include <memory>
#include "../../frontend/lexer/lexer.h"
#include "../stage.h"

namespace stages {

// This class is used to output medium test results for lexer
class LexerStage : public Stage {
 public:
  LexerStage() = delete;
  auto Run() -> void override {
    this->Collect();
    this->Print();
  }
  explicit LexerStage(std::unique_ptr<frontend::Lexer> lexer, std::ofstream &outfile)
      : lexer_(std::move(lexer)), outfile_(std::move(outfile)) {}

 private:
  std::unique_ptr<frontend::Lexer> lexer_;
  std::ofstream outfile_;
  std::deque<frontend::Token> tokens_;
  size_t max_tokentype_width_ = 9;
  size_t max_lexeme_width_ = 6;
  size_t max_line_ = 4;
  size_t max_column_ = 6;
  auto Collect() -> void;
  auto Print() -> void;
};

}  // namespace stages

#endif