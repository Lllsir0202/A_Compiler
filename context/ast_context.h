#ifndef CONTEXT_AST_CONTEXT_H
#define CONTEXT_AST_CONTEXT_H

#include "ast/ast_base.h"
namespace context {
/**
 * @brief Since every process in compiler will only need one Ast,
 * so we can control all Ast related information here.
 */
class AstContext {
 public:
  AstContext() = default;
  ~AstContext() = default;

  // Used to get and change the nodes' information in semantic analysis
  auto GetRootNode() -> frontend::ast::ASTNode * { return root_.get(); }
  // Used to get the root node in code generation stage, since it will never change the AST.
  auto GetRootNode() const -> const frontend::ast::ASTNode * { return root_.get(); }
  auto SetRootNode(frontend::ast::ASTNodePtr root) -> void { root_ = std::move(root); }

 private:
  // here we only need to hold the root node pointer
  // since it is a unique_ptr, so it will manage the memory automatically
  // we just need to provide access functions to get/set the root node pointer
  frontend::ast::ASTNodePtr root_;
};

}  // namespace context

#endif