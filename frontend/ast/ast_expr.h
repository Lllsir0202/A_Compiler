#ifndef FRONTEND_AST_AST_EXPR_H
#define FRONTEND_AST_AST_EXPR_H

#include "ast/astvisitor.h"
namespace frontend::ast {
// Define parent class for all expression nodes
class ExprNode : public VisitAble<ExprNode> {
 public:
  explicit ExprNode(int line, int column) : VisitAble<ExprNode>(line, column) {}
  ~ExprNode() override = default;
};

}  // namespace frontend::ast

#endif