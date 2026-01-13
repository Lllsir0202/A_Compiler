#ifndef FRONTEND_AST_VISITOR_H
#define FRONTEND_AST_VISITOR_H

#include "ast/ast_base.h"
namespace frontend::ast {

class ASTNode;
class RootNode;
class ExprNode;  // should not be called directly
class StmtNode;  // should not be called directly
class AddExprNode;
class MulExprNode;
class UnaryExprNode;
class PrimaryExprNode;
class LValExprNode;
class RelExprNode;
class EqExprNode;
class LAndExprNode;
class LOrExprNode;
class IntLiteralExprNode;
class FloatLiteralExprNode;

class ASTVisitor {
 public:
  virtual ~ASTVisitor() = default;

  // Visit methods for different AST node types can be declared here
  virtual void Visit(RootNode *root) = 0;
  virtual void Visit(AddExprNode *add_expr) = 0;
  virtual void Visit(MulExprNode *mul_expr) = 0;
  virtual void Visit(UnaryExprNode *unary_expr) = 0;
  virtual void Visit(PrimaryExprNode *primary_expr) = 0;
  virtual void Visit(LValExprNode *lval_expr) = 0;
  virtual void Visit(RelExprNode *rel_expr) = 0;
  virtual void Visit(EqExprNode *eq_expr) = 0;
  virtual void Visit(LAndExprNode *land_expr) = 0;
  virtual void Visit(LOrExprNode *lor_expr) = 0;
  virtual void Visit(IntLiteralExprNode *int_literal) = 0;
  virtual void Visit(FloatLiteralExprNode *float_literal) = 0;
};

template <typename Derived, typename Base = ASTNode>
class VisitAble : public Base {
 public:
  using Base::Base;
  void Accept(ASTVisitor *visitor) override { visitor->Visit(static_cast<Derived *>(this)); }
};

}  // namespace frontend::ast
#endif