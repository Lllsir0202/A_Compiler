#ifndef FRONTEND_AST_VISITOR_H
#define FRONTEND_AST_VISITOR_H

#include "ast/ast_base.h"
namespace frontend::ast {

class RootNode;
class ExprNode;  // should not be called directly
class StmtNode;  // should not be called directly

// Expression nodes
class AddExprNode;
class MulExprNode;
class UnaryExprNode;
class FuncCallExprNode;
class PrimaryExprNode;
class LValExprNode;
class RelExprNode;
class EqExprNode;
class LAndExprNode;
class LOrExprNode;
class IntLiteralExprNode;
class FloatLiteralExprNode;

// Statement nodes
class BlockStmtNode;
class AssignStmtNode;
class CommaStmtNode;
class IfStmtNode;
class WhileStmtNode;
class BreakStmtNode;
class ContinueStmtNode;
class ReturnStmtNode;

// Variable nodes includes decl and initializer
class GlobalVarDeclNode;
class GlobalVarDefNode;
class LocalVarDeclNode;
class LocalVarDefNode;
class InitListNode;
class InitValNode;

class ASTVisitor {
 public:
  virtual ~ASTVisitor() = default;

  // Visit methods for different AST node types can be declared here
  virtual void Visit(RootNode *root) = 0;
  // Visit methods for expression nodes
  virtual void Visit(AddExprNode *add_expr) = 0;
  virtual void Visit(MulExprNode *mul_expr) = 0;
  virtual void Visit(UnaryExprNode *unary_expr) = 0;
  virtual void Visit(FuncCallExprNode *func_call) = 0;
  virtual void Visit(PrimaryExprNode *primary_expr) = 0;
  virtual void Visit(LValExprNode *lval_expr) = 0;
  virtual void Visit(RelExprNode *rel_expr) = 0;
  virtual void Visit(EqExprNode *eq_expr) = 0;
  virtual void Visit(LAndExprNode *land_expr) = 0;
  virtual void Visit(LOrExprNode *lor_expr) = 0;
  virtual void Visit(IntLiteralExprNode *int_literal) = 0;
  virtual void Visit(FloatLiteralExprNode *float_literal) = 0;
  // Visit methods for statement nodes
  virtual void Visit(BlockStmtNode *block_stmt) = 0;
  virtual void Visit(AssignStmtNode *assign_stmt) = 0;
  virtual void Visit(CommaStmtNode *comma_stmt) = 0;
  virtual void Visit(IfStmtNode *if_stmt) = 0;
  virtual void Visit(WhileStmtNode *while_stmt) = 0;
  virtual void Visit(BreakStmtNode *break_stmt) = 0;
  virtual void Visit(ContinueStmtNode *continue_stmt) = 0;
  virtual void Visit(ReturnStmtNode *return_stmt) = 0;
  // Visit methods for variable nodes
  virtual void Visit(GlobalVarDeclNode *global_var_decl) = 0;
  virtual void Visit(GlobalVarDefNode *global_var_def) = 0;
  virtual void Visit(LocalVarDeclNode *local_var_decl) = 0;
  virtual void Visit(LocalVarDefNode *local_var_def) = 0;
  virtual void Visit(InitListNode *init_list) = 0;
  virtual void Visit(InitValNode *init_val) = 0;
};

template <typename Derived, typename Base = ASTNode>
class VisitAble : public Base {
 public:
  using Base::Base;
  void Accept(ASTVisitor *visitor) override { visitor->Visit(static_cast<Derived *>(this)); }
};

}  // namespace frontend::ast
#endif