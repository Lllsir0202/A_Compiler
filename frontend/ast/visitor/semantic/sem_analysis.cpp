#include "frontend/ast/visitor/semantic/sem_analysis.h"
#include <cstddef>
#include <stdexcept>
#include "ast/ast_base.h"
#include "ast/ast_decl.h"
#include "ast/ast_expr.h"
#include "ast/ast_stmt.h"
#include "ast/visitor/semantic/calculate.h"
#include "utils/type/type.h"

namespace frontend::ast {

/**
 * @brief Helper function to decide the result type of a binary expression based on the types of its operands
 * @param left the left operand of the binary expression
 * @param right the right operand of the binary expression
 * @return the result type of the binary expression
 */
inline auto SemanticAnalyser::DecideResultType(OpCode op, ExprNode *left, ExprNode *right) -> TypePtr {
  if (compiler_context_ == nullptr) {
    throw std::logic_error("Compiler context is null");
  }
  const bool lhs_is_float = left->GetType()->GetKind() == type::TypeKind::FLOAT;
  const bool rhs_is_float = right->GetType()->GetKind() == type::TypeKind::FLOAT;
  const bool lhs_is_int = left->GetType()->GetKind() == type::TypeKind::INT;
  const bool rhs_is_int = right->GetType()->GetKind() == type::TypeKind::INT;
  const bool lhs_is_bool = left->GetType()->GetKind() == type::TypeKind::BOOL;
  const bool rhs_is_bool = right->GetType()->GetKind() == type::TypeKind::BOOL;
  switch (op) {
    case OpCode::PLUS:
    case OpCode::SUB:
    case OpCode::MUL:
    case OpCode::DIV:
      // bool type is considered as integer to participate into calculation
      if ((lhs_is_int || lhs_is_bool || lhs_is_float) && (rhs_is_int || rhs_is_bool || rhs_is_float)) {
        if (lhs_is_float || rhs_is_float) {
          return compiler_context_->GetTypeContext()->GetFloatType();
        }
        return compiler_context_->GetTypeContext()->GetIntegerType();
      }
      throw std::runtime_error("invalid operand types for arithmetic operator");

    case OpCode::MOD:
      if ((lhs_is_int || lhs_is_bool) && (rhs_is_int || rhs_is_bool)) {
        return compiler_context_->GetTypeContext()->GetIntegerType();
      }
      throw std::runtime_error("invalid operand types for modulo operator");

    case OpCode::EQUAL:
    case OpCode::NEQUAL:
    case OpCode::LESS:
    case OpCode::GREATER:
    case OpCode::LEQUAL:
    case OpCode::GEQUAL:
    case OpCode::AND:
    case OpCode::OR:
      return compiler_context_->GetTypeContext()->GetBooleanType();

    default:
      throw std::logic_error("unsupported binary operator");
  }
}

/**
 * @brief Helper function used in unary expression
 */
inline auto SemanticAnalyser::DecideResultType(OpCode op, ExprNode *operand) -> TypePtr {
  switch (op) {
    case OpCode::NEGATIVE:
    case OpCode::POSITIVE:
      if (operand->GetType()->GetKind() != type::TypeKind::UNKNOWN) {
        return operand->GetType();
      }
    case OpCode::NOT:
      return compiler_context_->GetTypeContext()->GetBooleanType();
    default:
      throw std::logic_error("unsupported unary operator");
  }
}

// RootNode
void SemanticAnalyser::Visit(RootNode *root) {
  for (auto &child : root->SemanticGetchildren()) {
    child->Accept(this);
  }
}

// Following are the nodes which should not be accessed directly
void SemanticAnalyser::Visit([[maybe_unused]] ExprNode *expr) {
  throw std::logic_error("ExprNode is an abstract class and should not be visited directly");
}

void SemanticAnalyser::Visit([[maybe_unused]] StmtNode *stmt) {
  throw std::logic_error("StmtNode is an abstract class and should not be visited directly");
}

void SemanticAnalyser::Visit([[maybe_unused]] DeclNode *decl) {
  throw std::logic_error("DeclNode is an abstract class and should not be visited directly");
}

void SemanticAnalyser::Visit([[maybe_unused]] Initializer *initializer) {
  throw std::logic_error("Initializer is an abstract class and should not be visited directly");
}

// Begin with the easiest expression nodes
void SemanticAnalyser::Visit(IntLiteralExprNode *int_literal) {
  // Since int literal is definitely a constant expression
  // We can directly set its value and type here
  int_literal->GetValue().SetIntValue(int_literal->value_);
}

void SemanticAnalyser::Visit(FloatLiteralExprNode *float_literal) {
  // Since float literal is definitely a constant expression
  // We can directly set its value and type here
  float_literal->GetValue().SetFloatValue(float_literal->value_);
}

// Then it comes to binary expression nodes, which are algorithm operations
void SemanticAnalyser::Visit(BinaryExprNode *binary_expr) {
  // First visit left and right operands
  binary_expr->left_->Accept(this);
  binary_expr->right_->Accept(this);

  auto result_type = DecideResultType(binary_expr->operator_, binary_expr->left_.get(), binary_expr->right_.get());
  binary_expr->SetType(result_type);
  // Then we need to calculate the value if both operands are constant
  if (binary_expr->left_->IsConst() && binary_expr->right_->IsConst()) {
    FoldNodes(binary_expr, binary_expr->left_.get(), binary_expr->right_.get());
    binary_expr->SetConst(true);
  }
}

void SemanticAnalyser::Visit(UnaryExprNode *unary_expr) {
  // First visit the operand
  unary_expr->operand_->Accept(this);

  auto result_type = DecideResultType(unary_expr->operator_, unary_expr->operand_.get());
  unary_expr->SetType(result_type);
  // Then we need to calculate the value if the operand is constant
  if (unary_expr->operand_->IsConst()) {
    FoldNodes(unary_expr, unary_expr->operand_.get());
    unary_expr->SetConst(true);
  }
}

void SemanticAnalyser::Visit(FuncCallExprNode *func_call) {
  auto func_def = compiler_context_->GetSymbolTableContext()->LookupFunc(func_call->func_name_);
  if (func_def == nullptr) {
    throw std::runtime_error("function " + func_call->func_name_ + " is not defined");
  }

  // First visit all parameters
  for (auto &param : func_call->params_) {
    param->Accept(this);
  }
  // Then check the function definition and the passed parameters
  if (func_def->param_types_.size() != func_call->params_.size()) {
    throw std::runtime_error("function " + func_call->func_name_ + " expects " +
                             std::to_string(func_def->param_types_.size()) + " parameters, but got " +
                             std::to_string(func_call->params_.size()));
  }

  size_t param_count = func_def->param_types_.size();
  for (size_t i = 0; i < param_count; i++) {
    auto &def = func_def->param_types_[i];
    auto &param = func_call->params_[i];
    if (def != param->GetType()) {
      throw std::runtime_error("type mismatch for parameter " + std::to_string(i) + " of function " +
                               func_call->func_name_ + ": expected " + def->ToString() + ", but got " +
                               param->GetType()->ToString());
    }
  }

  auto return_type = func_def->return_type_;
  func_call->SetType(return_type);
}

void SemanticAnalyser::Visit(LValExprNode *LVal) {
  auto var_def = compiler_context_->GetSymbolTableContext()->LookupVar(LVal->ident_);
  if (var_def == nullptr) {
    throw std::runtime_error("variable " + LVal->ident_ + " is not defined");
  }
  auto def_indices = var_def->dimensions_;
  TypePtr result_type = nullptr;
  if (!def_indices.empty() && LVal->indices_.size() == def_indices.size()) {
    assert(var_def->type_->GetKind() == type::TypeKind::ARRAY);
    result_type = static_cast<type::ArrayType *>(var_def->type_)->GetElementType();
  }
  if (!def_indices.empty() && LVal->indices_.size() < def_indices.size()) {
    assert(var_def->type_->GetKind() == type::TypeKind::ARRAY);
    auto element_type = static_cast<type::ArrayType *>(var_def->type_)->GetElementType();
    result_type = compiler_context_->GetTypeContext()->GetPtrType(element_type);
  }
  if (def_indices.size() > LVal->indices_.size()) {
    throw std::runtime_error("Too many indices for variable " + LVal->ident_ + ": expected " +
                             std::to_string(def_indices.size()) + ", but got " + std::to_string(LVal->indices_.size()));
  }

  // Then we can set the type of the LVal expression
  if (result_type != nullptr) {
    LVal->SetType(result_type);
  }
  LVal->SetType(var_def->type_);
  LVal->SetConst(var_def->is_const_);
}

// Then we can visit stmt nodes
void SemanticAnalyser::Visit(BlockStmtNode *block_stmt) {
  for (auto &stmt : block_stmt->block_items_) {
    stmt->Accept(this);
  }
}

void SemanticAnalyser::Visit(AssignStmtNode *assign_stmt) {
  // First visit the left and right expressions
  assign_stmt->lval_->Accept(this);
  assign_stmt->expr_->Accept(this);
}

void SemanticAnalyser::Visit(ExprStmtNode *expr_stmt) {
  if (expr_stmt->expr_ != nullptr) {
    expr_stmt->expr_->Accept(this);
  }
}

void SemanticAnalyser::Visit(IfStmtNode *if_stmt) {
  if_stmt->condition_->Accept(this);
  if (if_stmt->then_branch_ != nullptr) {
    if_stmt->then_branch_->Accept(this);
  }
  if (if_stmt->else_branch_ != nullptr) {
    if_stmt->else_branch_->Accept(this);
  }
}

void SemanticAnalyser::Visit(WhileStmtNode *while_stmt) {
  while_stmt->condition_->Accept(this);
  if (while_stmt->body_ != nullptr) {
    while_stmt->body_->Accept(this);
  }
}

void SemanticAnalyser::Visit([[maybe_unused]] BreakStmtNode *break_stmt) {
  // No need to do anything for break statement
}

void SemanticAnalyser::Visit([[maybe_unused]] ContinueStmtNode *continue_stmt) {
  // No need to do anything for continue statement
}

void SemanticAnalyser::Visit(ReturnStmtNode *return_stmt) {
  if (return_stmt->expr_ != nullptr) {
    return_stmt->expr_->Accept(this);
  }
}

// Then Declaration nodes

}  // namespace frontend::ast