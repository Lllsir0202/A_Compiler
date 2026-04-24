#ifndef MIDDLEEND_BUILDER_MLIR_MLIRVISITOR_H
#define MIDDLEEND_BUILDER_MLIR_MLIRVISITOR_H
#include "ast/ast_expr.h"
#include "compiler_context.h"
#include "flatten/flatten_initializer.h"
#include "middleend/builder/irvisitor.h"
#include "mlir/utils/value/value.h"

namespace middleend {
/**
 * @brief MLIRVisitor is the concrete visitor for MLIR builder,
 * it will traverse the AST and generate the corresponding MLIR IR
 * which is represented by the ``Dialect::base`` dialect.
 * Then we can do optimization and code generation on top of the base dialect.
 */
class MLIRVisitor : public IRVisitor {
 public:
  ~MLIRVisitor() override { compiler_context_ = nullptr; }

  // RootNode
  void Visit(frontend::ast::RootNode *root) override;
  // Should not be called directly, since they are abstract classes
  void Visit(frontend::ast::ExprNode *expr) override;
  void Visit(frontend::ast::StmtNode *stmt) override;
  void Visit(frontend::ast::Initializer *initializer) override;
  void Visit(frontend::ast::DeclNode *decl) override;
  // Expression nodes
  void Visit(frontend::ast::BinaryExprNode *binary_expr) override;
  void Visit(frontend::ast::UnaryExprNode *unary_expr) override;
  void Visit(frontend::ast::FuncCallExprNode *func_call) override;
  void Visit(frontend::ast::LValExprNode *lval_expr) override;
  void Visit(frontend::ast::IntLiteralExprNode *int_literal) override;
  void Visit(frontend::ast::FloatLiteralExprNode *float_literal) override;
  // Statement nodes
  void Visit(frontend::ast::BlockStmtNode *block_stmt) override;
  void Visit(frontend::ast::AssignStmtNode *assign_stmt) override;
  void Visit(frontend::ast::ExprStmtNode *expr_stmt) override;
  void Visit(frontend::ast::IfStmtNode *if_stmt) override;
  void Visit(frontend::ast::WhileStmtNode *while_stmt) override;
  void Visit(frontend::ast::BreakStmtNode *break_stmt) override;
  void Visit(frontend::ast::ContinueStmtNode *continue_stmt) override;
  void Visit(frontend::ast::ReturnStmtNode *return_stmt) override;
  // Variable nodes includes decl and initializer
  void Visit(frontend::ast::VarDefNode *var_def) override;
  void Visit(frontend::ast::GlobalVarDeclNode *global_var_decl) override;
  void Visit(frontend::ast::LocalVarDeclNode *local_var_decl) override;
  void Visit(frontend::ast::InitListNode *init_list) override;
  void Visit(frontend::ast::InitValNode *init_val) override;
  // Function nodes
  void Visit(frontend::ast::FuncFParamNode *func_fparam) override;
  void Visit(frontend::ast::FuncDefNode *func_def) override;

 private:
  /**
   * @brief Since the MLIRVisitor is tightly coupled with the MLIRBuilder,
   * and there shall be no other class need to call the Visit functions of this class,
   * so we can just set the constructor to private
   * and let the MLIRBuilder be the friend class of this class.
   */
  explicit MLIRVisitor(context::CompilerContext *compiler_context)
      : IRVisitor(compiler_context), flatten_helper_(compiler_context) {}
  friend class MLIRBuilder;

  // Helper functions for handing out operator
  inline auto HandleBinaryOperator(frontend::ast::BinaryExprNode *binary_expr,
                                   context::CompilerContext *compiler_context) -> mlir::Value *;
  inline auto HandleUnaryOperator(frontend::ast::UnaryExprNode *unary_expr,
                                  context::CompilerContext *compiler_context) -> mlir::Value *;
  utils::flatten::FlattenHelper flatten_helper_;
};

}  // namespace middleend

#endif