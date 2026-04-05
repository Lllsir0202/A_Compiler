#ifndef FRONTEND_AST_VISITOR_SEMANTIC_SEM_ANALYSIS_H
#define FRONTEND_AST_VISITOR_SEMANTIC_SEM_ANALYSIS_H
#include <fstream>
#include "ast/ast_base.h"
#include "context/compiler_context.h"
#include "context/symboltable_context.h"
#include "frontend/ast/ast_decl.h"
#include "frontend/ast/ast_expr.h"
#include "frontend/ast/ast_spec.h"
#include "frontend/ast/ast_stmt.h"
#include "frontend/ast/astvisitor.h"
#include "type/type.h"
#include "utils/flatten/astinit.h"
#include "utils/flatten/flatten_initializer.h"
#include "utils/flatten/initializer.h"

namespace frontend::ast {
class SemanticAnalyser : public ASTVisitor {
 public:
  // Constructor
  explicit SemanticAnalyser(context::CompilerContext &compiler_context)
      : compiler_context_(&compiler_context), flatten_helper_(&compiler_context) {}
  ~SemanticAnalyser() override {
    if (compiler_context_ != nullptr) {
      compiler_context_->GetSymbolTableContext()->ResetCurrentFunc();
      compiler_context_ = nullptr;
    }
  };

  // Provide analyse method to start the semantic analysis process
  auto Analyse() -> int;

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
  utils::flatten::FlattenHelper flatten_helper_;
  /**
   * @brief DecideResultType function will decide the result type of a binary expression based on the types of its
   * operands
   * @note even though the operands maybe variables, we still need to check the type of the result
   */
  inline auto DecideResultType(OpCode op, ExprNode *left, ExprNode *right) -> TypePtr;
  inline auto DecideResultType(OpCode op, ExprNode *operand) -> TypePtr;
  inline auto Linear(const std::vector<int> &dimensions, const std::vector<int> &indices) -> int;
  /**
   * @brief Normalize function will fill the default value,
   * and will also cast the value to the target type(if needed, the var_def's type) value.
   * @note override functions to deal with array initializer and a const initializer.
   */
  inline auto Normalize(symboltable::VarEntry *entry, std::unique_ptr<initializer::ArrayInitializer> &init) -> void;
  inline auto Normalize(symboltable::VarEntry *entry, std::unique_ptr<initializer::ConstInitializer> &init) -> void;

  auto CheckCast(TypePtr from, TypePtr to) -> bool {
    bool from_is_basic = from->GetKind() == type::TypeKind::INT || from->GetKind() == type::TypeKind::FLOAT ||
                         from->GetKind() == type::TypeKind::BOOL;
    bool to_is_basic = to->GetKind() == type::TypeKind::INT || to->GetKind() == type::TypeKind::FLOAT ||
                       to->GetKind() == type::TypeKind::BOOL;
    return (from_is_basic && to_is_basic) || from == to;
  };

  auto CheckZero(Value &value) -> bool {
    if (value.GetType() == ValueType::INT) {
      return value.GetIntValue() == 0;
    }
    if (value.GetType() == ValueType::FLOAT) {
      return value.GetFloatValue() == 0.0F;
    }
    if (value.GetType() == ValueType::BOOL) {
      return !value.GetBoolValue();
    }
    compiler_context_->ReportError("Unsupported value type for zero check");
    return false;
  }

#define Debug
#ifdef Debug
  std::ofstream debug_file_{"normalized_initializer.txt", std::ios::out};
#endif
};
}  // namespace frontend::ast
#endif