#ifndef FRONTEND_AST_VISITOR_H
#define FRONTEND_AST_VISITOR_H

#include "ast/ast_base.h"
namespace frontend::ast {

class ASTNode;

class ASTVisitor {
 public:
  virtual ~ASTVisitor() = default;

  // Visit methods for different AST node types can be declared here
  virtual void Visit() = 0;
};

template <typename T>
class VisitAble : public ASTNode {
 public:
  using ASTNode::ASTNode;
  void Accept(ASTVisitor *visitor) override { visitor->Visit(static_cast<T *>(this)); }
};

}  // namespace frontend::ast
#endif