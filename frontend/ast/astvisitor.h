#ifndef FRONTEND_AST_VISITOR_H
#define FRONTEND_AST_VISITOR_H

#include "ast/ast_base.h"
namespace frontend::ast {

class RootNode;
class ExprNode;  // should not be called directly
class StmtNode;  // should not be called directly

// Expression nodes
class BinaryExprNode;
class UnaryExprNode;
class FuncCallExprNode;
class LValExprNode;
class IntLiteralExprNode;
class FloatLiteralExprNode;

// Statement nodes
class BlockStmtNode;
class AssignStmtNode;
class ExprStmtNode;
class IfStmtNode;
class WhileStmtNode;
class BreakStmtNode;
class ContinueStmtNode;
class ReturnStmtNode;

// Variable nodes includes decl and initializer
class VarDefNode;
class GlobalVarDeclNode;
class GlobalVarDefNode;
class LocalVarDeclNode;
class LocalVarDefNode;
class InitListNode;
class InitValNode;
class FuncFParamNode;
class FuncDefNode;

// There are two kinds of visitors: mutable and const visitors
class ASTVisitor {
 public:
  virtual ~ASTVisitor() = default;

  // Visit methods for different AST node types can be declared here
  virtual void Visit(RootNode *root) = 0;
  // Visit methods for expression nodes
  virtual void Visit(BinaryExprNode *binary_expr) = 0;
  virtual void Visit(UnaryExprNode *unary_expr) = 0;
  virtual void Visit(FuncCallExprNode *func_call) = 0;
  virtual void Visit(LValExprNode *lval_expr) = 0;
  virtual void Visit(IntLiteralExprNode *int_literal) = 0;
  virtual void Visit(FloatLiteralExprNode *float_literal) = 0;
  // Visit methods for statement nodes
  virtual void Visit(BlockStmtNode *block_stmt) = 0;
  virtual void Visit(AssignStmtNode *assign_stmt) = 0;
  virtual void Visit(ExprStmtNode *expr_stmt) = 0;
  virtual void Visit(IfStmtNode *if_stmt) = 0;
  virtual void Visit(WhileStmtNode *while_stmt) = 0;
  virtual void Visit(BreakStmtNode *break_stmt) = 0;
  virtual void Visit(ContinueStmtNode *continue_stmt) = 0;
  virtual void Visit(ReturnStmtNode *return_stmt) = 0;
  // Visit methods for variable nodes
  virtual void Visit(VarDefNode *var_def) = 0;
  virtual void Visit(GlobalVarDeclNode *global_var_decl) = 0;
  virtual void Visit(GlobalVarDefNode *global_var_def) = 0;
  virtual void Visit(LocalVarDeclNode *local_var_decl) = 0;
  virtual void Visit(LocalVarDefNode *local_var_def) = 0;
  virtual void Visit(InitListNode *init_list) = 0;
  virtual void Visit(InitValNode *init_val) = 0;
  virtual void Visit(FuncFParamNode *func_fparam) = 0;
  virtual void Visit(FuncDefNode *func_def) = 0;
};

class ConstASTVisitor {
 public:
  virtual ~ConstASTVisitor() = default;

  // Visit methods for different AST node types can be declared here
  virtual void Visit(const RootNode *root) = 0;
  // Visit methods for expression nodes
  virtual void Visit(const BinaryExprNode *binary_expr) = 0;
  virtual void Visit(const UnaryExprNode *unary_expr) = 0;
  virtual void Visit(const FuncCallExprNode *func_call) = 0;
  virtual void Visit(const LValExprNode *lval_expr) = 0;
  virtual void Visit(const IntLiteralExprNode *int_literal) = 0;
  virtual void Visit(const FloatLiteralExprNode *float_literal) = 0;
  // Visit methods for statement nodes
  virtual void Visit(const BlockStmtNode *block_stmt) = 0;
  virtual void Visit(const AssignStmtNode *assign_stmt) = 0;
  virtual void Visit(const ExprStmtNode *expr_stmt) = 0;
  virtual void Visit(const IfStmtNode *if_stmt) = 0;
  virtual void Visit(const WhileStmtNode *while_stmt) = 0;
  virtual void Visit(const BreakStmtNode *break_stmt) = 0;
  virtual void Visit(const ContinueStmtNode *continue_stmt) = 0;
  virtual void Visit(const ReturnStmtNode *return_stmt) = 0;
  // Visit methods for variable nodes
  virtual void Visit(const VarDefNode *var_def) = 0;
  virtual void Visit(const GlobalVarDeclNode *global_var_decl) = 0;
  virtual void Visit(const GlobalVarDefNode *global_var_def) = 0;
  virtual void Visit(const LocalVarDeclNode *local_var_decl) = 0;
  virtual void Visit(const LocalVarDefNode *local_var_def) = 0;
  virtual void Visit(const InitListNode *init_list) = 0;
  virtual void Visit(const InitValNode *init_val) = 0;
  virtual void Visit(const FuncFParamNode *func_fparam) = 0;
  virtual void Visit(const FuncDefNode *func_def) = 0;
};

template <typename Derived, typename Base = ASTNode>
class VisitAble : public Base {
 public:
  using Base::Base;
  void Accept(ASTVisitor *visitor) override { visitor->Visit(static_cast<Derived *>(this)); }
  void Accept(ConstASTVisitor *visitor) const override { visitor->Visit(static_cast<const Derived *>(this)); }
};

}  // namespace frontend::ast
#endif