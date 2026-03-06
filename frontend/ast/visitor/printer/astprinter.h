#ifndef FRONTEND_AST_VISITOR_PRINTER_ASTPRINTER_H
#define FRONTEND_AST_VISITOR_PRINTER_ASTPRINTER_H
#include <fstream>
#include "frontend/ast/ast_base.h"
#include "frontend/ast/ast_decl.h"
#include "frontend/ast/ast_spec.h"
#include "frontend/ast/astvisitor.h"

namespace frontend::ast {
class ASTPrinter : public ConstASTVisitor {
 public:
  // Constructor
  explicit ASTPrinter(std::ofstream *out) : out_(out) {}
  ~ASTPrinter() override { out_ = nullptr; }
  // RootNode
  void Visit(const RootNode *root) override;
  // Should not be called directly, since they are abstract classes
  void Visit(const ExprNode *expr) override;
  void Visit(const StmtNode *stmt) override;
  void Visit(const Initializer *initializer) override;
  void Visit(const DeclNode *decl) override;
  // Expression nodes
  void Visit(const BinaryExprNode *binary_expr) override;
  void Visit(const UnaryExprNode *unary_expr) override;
  void Visit(const FuncCallExprNode *func_call) override;
  void Visit(const LValExprNode *lval_expr) override;
  void Visit(const IntLiteralExprNode *int_literal) override;
  void Visit(const FloatLiteralExprNode *float_literal) override;
  // Statement nodes
  void Visit(const BlockStmtNode *block_stmt) override;
  void Visit(const AssignStmtNode *assign_stmt) override;
  void Visit(const ExprStmtNode *expr_stmt) override;
  void Visit(const IfStmtNode *if_stmt) override;
  void Visit(const WhileStmtNode *while_stmt) override;
  void Visit(const BreakStmtNode *break_stmt) override;
  void Visit(const ContinueStmtNode *continue_stmt) override;
  void Visit(const ReturnStmtNode *return_stmt) override;
  // Variable nodes includes decl and initializer
  void Visit(const VarDefNode *var_def) override;
  void Visit(const GlobalVarDeclNode *global_var_decl) override;
  void Visit(const LocalVarDeclNode *local_var_decl) override;
  void Visit(const InitListNode *init_list) override;
  void Visit(const InitValNode *init_val) override;
  // Function nodes
  void Visit(const FuncFParamNode *func_fparam) override;
  void Visit(const FuncDefNode *func_def) override;

 private:
  int indent_level_ = 0;
  void PrintIndent() const;
  std::ofstream *out_;
};

}  // namespace frontend::ast
#endif