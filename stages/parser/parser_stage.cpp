#include "stages/parser/parser_stage.h"
#include <cassert>

namespace stages {
void ParserStage::Run() {
  /**
   * @brief Run the parser stage
   * @use: Run from the ast root to print the total AST structure.
   */
  assert(compiler_context_ != nullptr && "Compiler context is null in ParserStage");
  assert(ast_printer_ != nullptr && "AST printer is null in ParserStage");
  const auto root = compiler_context_->GetAstContext()->GetRootNode();
  assert(root != nullptr && "Root node is null in ParserStage");
  root->Accept(ast_printer_.get());
}
}  // namespace stages