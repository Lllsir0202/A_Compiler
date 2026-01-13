#ifndef FRONTEND_AST_AST_SPEC_H
#define FRONTEND_AST_AST_SPEC_H

#include <vector>
#include "ast/ast_base.h"
#include "ast/astvisitor.h"

namespace frontend::ast {

// Here we define some special AST node types
/**
 * @brief Some special AST node type such as Root
 */

/**
 * @brief The root node of the AST
 * This node represents the entire program
 */
class RootNode : public VisitAble<RootNode> {
 public:
  explicit RootNode() = default;
  explicit RootNode(std::vector<ASTNodePtr> children) : children_(std::move(children)) {}
  ~RootNode() override = default;
  // provide methods to manage children but I think there's no need to delete a child.
  [[maybe_unused]] auto AddChild(ASTNodePtr child) -> void { children_.push_back(std::move(child)); }
  [[maybe_unused]] auto GetChildren() const -> const std::vector<ASTNodePtr> & { return children_; }

 protected:
  std::vector<ASTNodePtr> children_;
};

}  // namespace frontend::ast

#endif