#ifndef PARSER_STAGE_H
#define PARSER_STAGE_H
#include <memory>
#include "context/compiler_context.h"
#include "frontend/ast/visitor/printer/astprinter.h"
#include "stages/stage.h"

namespace stages {
// This class is used to output medium test results for parser
class ParserStage : public Stage {
 public:
  void Run() override;

  explicit ParserStage(context::CompilerContext *compiler_context,
                       std::unique_ptr<frontend::ast::ASTPrinter> ast_printer)
      : compiler_context_(compiler_context), ast_printer_(std::move(ast_printer)) {}

  ~ParserStage() override { compiler_context_ = nullptr; }

 private:
  context::CompilerContext *compiler_context_;
  std::unique_ptr<frontend::ast::ASTPrinter> ast_printer_;
};
}  // namespace stages

#endif