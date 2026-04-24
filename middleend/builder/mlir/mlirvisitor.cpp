#include "middleend/builder/mlir/mlirvisitor.h"
#include <map>
#include "ast/ast_utils.h"
#include "frontend/ast/ast_spec.h"
#include "middleend/mlir/dialect/arithm/binaryop.h"
#include "middleend/mlir/dialect/arithm/unaryop.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/region.h"
#define GET_MLIR_CONTEXT() compiler_context_->GetIRContext()->GetMLIRContext()
namespace middleend {

#include "middleend/builder/mlir/binary_handler.def"
/**
 * @brief The Helper functions for handing out operators
 */
inline auto MLIRVisitor::HandleBinaryOperator(frontend::ast::OpCode op_code,
                                              context::CompilerContext *compiler_context) -> void {
  switch (op_code) {
    BINARY_OPERATOR_HANDLER(PLUS, Add)
    BINARY_OPERATOR_HANDLER(SUB, Sub)
    BINARY_OPERATOR_HANDLER(MUL, Mul)
    BINARY_OPERATOR_HANDLER(DIV, Div)
    BINARY_OPERATOR_HANDLER(MOD, Mod)
    BINARY_OPERATOR_HANDLER(LESS, Less)
    BINARY_OPERATOR_HANDLER(LEQUAL, LessEqual)
    BINARY_OPERATOR_HANDLER(GREATER, Greater)
    BINARY_OPERATOR_HANDLER(GEQUAL, GreaterEqual)
    BINARY_OPERATOR_HANDLER(AND, And)
    BINARY_OPERATOR_HANDLER(OR, Or)
    default:
      compiler_context->ReportError("Unsupported binary operator.");
  }
}

#include "middleend/builder/mlir/unary_handler.def"
inline auto MLIRVisitor::HandleUnaryOperator(frontend::ast::OpCode op_code,
                                             context::CompilerContext *compiler_context) -> void {
  /**
   * @brief Since the unary positive is the same as doing nothing,
   * we will just do nothing when we encounter a unary positive operator
   */
  switch (op_code) {
    UNARY_OPERATOR_HANDLER(NOT, Not)
    UNARY_OPERATOR_HANDLER(NEGATIVE, Neg)
    default:
      compiler_context->ReportError("Unsupported unary operator.");
  }
}

// TODO(loki): implement the MLIRVisitor::Visitor functions to generate MLIR IR
/**
 * @brief This file is the implementation of the MLIRVisitor class,
 * which will be used to traverse the AST and generate the MLIR IR.
 */

// RootNode
void MLIRVisitor::Visit([[maybe_unused]] const frontend::ast::RootNode *root) {
  // Create the module operation, which is the top-level operation in MLIR
  auto module_op = GET_MLIR_CONTEXT()->CreateOperation<middleend::mlir::ModuleOp>();
  GET_MLIR_CONTEXT()->SetInsertPoint(module_op->regions_[0]->blocks_[0]);
  root->Accept(this);
}
// Should not be called directly, since they are abstract classes
void MLIRVisitor::Visit([[maybe_unused]] const frontend::ast::ExprNode *expr) {
  compiler_context_->ReportError("ExprNode: Should not visit an abstract expression node directly.");
}
void MLIRVisitor::Visit([[maybe_unused]] const frontend::ast::StmtNode *stmt) {
  compiler_context_->ReportError("StmtNode: Should not visit an abstract statement node directly.");
}
void MLIRVisitor::Visit([[maybe_unused]] const frontend::ast::Initializer *initializer) {
  compiler_context_->ReportError("Initializer: Should not visit an abstract initializer node directly.");
}
void MLIRVisitor::Visit([[maybe_unused]] const frontend::ast::DeclNode *decl) {
  compiler_context_->ReportError("DeclNode: Should not visit an abstract declaration node directly.");
}
// Expression nodes
void MLIRVisitor::Visit([[maybe_unused]] const frontend::ast::BinaryExprNode *binary_expr) {
  auto lhs = binary_expr->left_.get();
  lhs->Accept(this);
  auto rhs = binary_expr->right_.get();
  rhs->Accept(this);
  /**
   * @brief the lhs and rhs has been visited,
   * since the lhs is visited first, then the rhs,
   * so the top will be the rhs.
   * @example:
   * a + b -> visit(a) -> push a -> visit(b) -> push b
   * in stack : bottom ->[a, b]<- top
   * so get value should be: rhs = pop , lhs = pop
   */
  HandleBinaryOperator(binary_expr->operator_, compiler_context_);
}
void MLIRVisitor::Visit([[maybe_unused]] const frontend::ast::UnaryExprNode *unary_expr) {
  auto operand = unary_expr->operand_.get();
  operand->Accept(this);
  /** * @brief the operand has been visited,
   * and the result of the operand shall be stored in the MLIR StateContext,
   * also it should be the top value */
  HandleUnaryOperator(unary_expr->operator_, compiler_context_);
}
void MLIRVisitor::Visit([[maybe_unused]] const frontend::ast::FuncCallExprNode *func_call) {}
void MLIRVisitor::Visit([[maybe_unused]] const frontend::ast::LValExprNode *lval_expr) {}
void MLIRVisitor::Visit([[maybe_unused]] const frontend::ast::IntLiteralExprNode *int_literal) {}
void MLIRVisitor::Visit([[maybe_unused]] const frontend::ast::FloatLiteralExprNode *float_literal) {}
// Statement nodes
void MLIRVisitor::Visit([[maybe_unused]] const frontend::ast::BlockStmtNode *block_stmt) {}
void MLIRVisitor::Visit([[maybe_unused]] const frontend::ast::AssignStmtNode *assign_stmt) {}
void MLIRVisitor::Visit([[maybe_unused]] const frontend::ast::ExprStmtNode *expr_stmt) {}
void MLIRVisitor::Visit([[maybe_unused]] const frontend::ast::IfStmtNode *if_stmt) {}
void MLIRVisitor::Visit([[maybe_unused]] const frontend::ast::WhileStmtNode *while_stmt) {}
void MLIRVisitor::Visit([[maybe_unused]] const frontend::ast::BreakStmtNode *break_stmt) {}
void MLIRVisitor::Visit([[maybe_unused]] const frontend::ast::ContinueStmtNode *continue_stmt) {}
void MLIRVisitor::Visit([[maybe_unused]] const frontend::ast::ReturnStmtNode *return_stmt) {}
// Variable nodes includes decl and initializer
void MLIRVisitor::Visit([[maybe_unused]] const frontend::ast::VarDefNode *var_def) {}
void MLIRVisitor::Visit([[maybe_unused]] const frontend::ast::GlobalVarDeclNode *global_var_decl) {}
void MLIRVisitor::Visit([[maybe_unused]] const frontend::ast::LocalVarDeclNode *local_var_decl) {}
void MLIRVisitor::Visit([[maybe_unused]] const frontend::ast::InitListNode *init_list) {}
void MLIRVisitor::Visit([[maybe_unused]] const frontend::ast::InitValNode *init_val) {}
// Function nodes
void MLIRVisitor::Visit([[maybe_unused]] const frontend::ast::FuncFParamNode *func_fparam) {}
void MLIRVisitor::Visit([[maybe_unused]] const frontend::ast::FuncDefNode *func_def) {}

}  // namespace middleend