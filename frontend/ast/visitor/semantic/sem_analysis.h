#ifndef FRONTEND_AST_VISITOR_SEMANTIC_SEM_ANALYSIS_H
#define FRONTEND_AST_VISITOR_SEMANTIC_SEM_ANALYSIS_H
#include "ast/ast_base.h"
#include "context/compiler_context.h"
#include "frontend/ast/ast_decl.h"
#include "frontend/ast/ast_expr.h"
#include "frontend/ast/ast_spec.h"
#include "frontend/ast/ast_stmt.h"
#include "frontend/ast/astvisitor.h"

namespace frontend::ast {
class SemanticAnalyser : public ASTVisitor {
 public:
  // Constructor
  explicit SemanticAnalyser(context::CompilerContext &compiler_context) : compiler_context_(&compiler_context) {}
  ~SemanticAnalyser() override { compiler_context_ = nullptr; };
  // RootNode
  void Visit(RootNode *root) override;
  // Should not be called directly, since they are abstract classes
  void Visit(ExprNode *expr) override;
  void Visit(StmtNode *stmt) override;
  void Visit(Initializer *initializer) override;
  void Visit(DeclNode *decl) override;
  // Expression nodes
  void Visit(BinaryExprNode *binary_expr) override;
  void Visit(UnaryExprNode *unary_expr) override;
  void Visit(FuncCallExprNode *func_call) override;
  void Visit(LValExprNode *lval_expr) override;
  void Visit(IntLiteralExprNode *int_literal) override;
  void Visit(FloatLiteralExprNode *float_literal) override;
  // Statement nodes
  void Visit(BlockStmtNode *block_stmt) override;
  void Visit(AssignStmtNode *assign_stmt) override;
  void Visit(ExprStmtNode *expr_stmt) override;
  void Visit(IfStmtNode *if_stmt) override;
  void Visit(WhileStmtNode *while_stmt) override;
  void Visit(BreakStmtNode *break_stmt) override;
  void Visit(ContinueStmtNode *continue_stmt) override;
  void Visit(ReturnStmtNode *return_stmt) override;
  // Variable nodes includes decl and initializer
  void Visit(VarDefNode *var_def) override;
  void Visit(GlobalVarDeclNode *global_var_decl) override;
  void Visit(LocalVarDeclNode *local_var_decl) override;
  void Visit(InitListNode *init_list) override;
  void Visit(InitValNode *init_val) override;
  // Function nodes
  void Visit(FuncFParamNode *func_fparam) override;
  void Visit(FuncDefNode *func_def) override;

 private:
  context::CompilerContext *compiler_context_;
  inline auto DecideResultType(OpCode op, ExprNode *left, ExprNode *right) -> TypePtr;
  inline auto DecideResultType(OpCode op, ExprNode *operand) -> TypePtr;
};
}  // namespace frontend::ast
#endif