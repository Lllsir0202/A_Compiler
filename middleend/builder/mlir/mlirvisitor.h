#ifndef MIDDLEEND_BUILDER_MLIR_MLIRVISITOR_H
#define MIDDLEEND_BUILDER_MLIR_MLIRVISITOR_H
#include "middleend/builder/irvisitor.h"

namespace middleend {
/**
 * @brief MLIRVisitor is the concrete visitor for MLIR builder,
 * it will traverse the AST and generate the corresponding MLIR IR
 * which is represented by the ``Dialect::base`` dialect.
 * Then we can do optimization and code generation on top of the base dialect.
 */
class MLIRVisitor : public IRVisitor {
 public:
  explicit MLIRVisitor(context::CompilerContext *compiler_context) : IRVisitor(compiler_context) {}
  ~MLIRVisitor() override { compiler_context_ = nullptr; }

  // RootNode
  void Visit(const frontend::ast::RootNode *root) override;
  // Should not be called directly, since they are abstract classes
  void Visit(const frontend::ast::ExprNode *expr) override;
  void Visit(const frontend::ast::StmtNode *stmt) override;
  void Visit(const frontend::ast::Initializer *initializer) override;
  void Visit(const frontend::ast::DeclNode *decl) override;
  // Expression nodes
  void Visit(const frontend::ast::BinaryExprNode *binary_expr) override;
  void Visit(const frontend::ast::UnaryExprNode *unary_expr) override;
  void Visit(const frontend::ast::FuncCallExprNode *func_call) override;
  void Visit(const frontend::ast::LValExprNode *lval_expr) override;
  void Visit(const frontend::ast::IntLiteralExprNode *int_literal) override;
  void Visit(const frontend::ast::FloatLiteralExprNode *float_literal) override;
  // Statement nodes
  void Visit(const frontend::ast::BlockStmtNode *block_stmt) override;
  void Visit(const frontend::ast::AssignStmtNode *assign_stmt) override;
  void Visit(const frontend::ast::ExprStmtNode *expr_stmt) override;
  void Visit(const frontend::ast::IfStmtNode *if_stmt) override;
  void Visit(const frontend::ast::WhileStmtNode *while_stmt) override;
  void Visit(const frontend::ast::BreakStmtNode *break_stmt) override;
  void Visit(const frontend::ast::ContinueStmtNode *continue_stmt) override;
  void Visit(const frontend::ast::ReturnStmtNode *return_stmt) override;
  // Variable nodes includes decl and initializer
  void Visit(const frontend::ast::VarDefNode *var_def) override;
  void Visit(const frontend::ast::GlobalVarDeclNode *global_var_decl) override;
  void Visit(const frontend::ast::LocalVarDeclNode *local_var_decl) override;
  void Visit(const frontend::ast::InitListNode *init_list) override;
  void Visit(const frontend::ast::InitValNode *init_val) override;
  // Function nodes
  void Visit(const frontend::ast::FuncFParamNode *func_fparam) override;
  void Visit(const frontend::ast::FuncDefNode *func_def) override;
};

}  // namespace middleend

#endif