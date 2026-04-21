#include "middleend/builder/mlir/mlirvisitor.h"

namespace middleend {
// TODO(loki): implement the MLIRVisitor::Visitor functions to generate MLIR IR
/**
 * @brief This file is the implementation of the MLIRVisitor class,
 * which will be used to traverse the AST and generate the MLIR IR.
 */

// RootNode
void MLIRVisitor::Visit([[maybe_unused]] const frontend::ast::RootNode *root) {}
// Should not be called directly, since they are abstract classes
void MLIRVisitor::Visit([[maybe_unused]] const frontend::ast::ExprNode *expr) {}
void MLIRVisitor::Visit([[maybe_unused]] const frontend::ast::StmtNode *stmt) {}
void MLIRVisitor::Visit([[maybe_unused]] const frontend::ast::Initializer *initializer) {}
void MLIRVisitor::Visit([[maybe_unused]] const frontend::ast::DeclNode *decl) {}
// Expression nodes
void MLIRVisitor::Visit([[maybe_unused]] const frontend::ast::BinaryExprNode *binary_expr) {}
void MLIRVisitor::Visit([[maybe_unused]] const frontend::ast::UnaryExprNode *unary_expr) {}
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