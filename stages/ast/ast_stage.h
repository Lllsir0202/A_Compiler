#ifndef STAGES_AST_AST_STAGE_H
#define STAGES_AST_AST_STAGE_H
#include "context/compiler_context.h"
#include "frontend/ast/visitor/printer/astprinter.h"
#include "stages/stage.h"

namespace stages {
/**
 * @brief ASTStage representing the stage of AST
 * which has been through semantic analysis and type checking,
 * and the AST is semantically correct.
 */
class ASTStage : public Stage {
 public:
  void Run() override;

  explicit ASTStage(context::CompilerContext *compiler_context, std::unique_ptr<frontend::ast::ASTPrinter> ast_printer)
      : compiler_context_(compiler_context), ast_printer_(std::move(ast_printer)) {}

  ~ASTStage() override { compiler_context_ = nullptr; }

 private:
  context::CompilerContext *compiler_context_;
  std::unique_ptr<frontend::ast::ASTPrinter> ast_printer_;
};
}  // namespace stages

#endif