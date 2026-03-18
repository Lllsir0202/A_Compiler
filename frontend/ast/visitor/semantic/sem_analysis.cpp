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
      compiler_context_->ReportError("invalid operand types for arithmetic operator: " + left->GetType()->ToString() +
                                     " and " + right->GetType()->ToString());
    case OpCode::MOD:
      if ((lhs_is_int || lhs_is_bool) && (rhs_is_int || rhs_is_bool)) {
        return compiler_context_->GetTypeContext()->GetIntegerType();
      }
      compiler_context_->ReportError("invalid operand types for modulo operator: " + left->GetType()->ToString() +
                                     " and " + right->GetType()->ToString());

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
      compiler_context_->ReportError("unsupported binary operator");
      return compiler_context_->GetTypeContext()->GetUnknownType();
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
      compiler_context_->ReportError("unsupported unary operator");
      return compiler_context_->GetTypeContext()->GetUnknownType();
  }
}

auto SemanticAnalyser::Analyse() -> int {
  if (compiler_context_ == nullptr) {
    throw std::logic_error("Compiler context is null");
  }
  try {
    auto root = compiler_context_->GetAstContext()->GetRootNode();
    root->Accept(this);
  } catch (const std::runtime_error &e) {
    // Has exception in semantic analysis, return 1 to indicate failure
    compiler_context_->GetSymbolTableContext()->ResetCurrentFunc();
    compiler_context_ = nullptr;
    return 1;
  } catch (const std::logic_error &e) {
    compiler_context_->GetSymbolTableContext()->ResetCurrentFunc();
    compiler_context_ = nullptr;
    return 2;
  } catch (...) {
    compiler_context_->GetSymbolTableContext()->ResetCurrentFunc();
    compiler_context_ = nullptr;
    return 3;
  }

  // succeed to analyse
  compiler_context_->GetSymbolTableContext()->ResetCurrentFunc();
  compiler_context_ = nullptr;
  return 0;
}

// RootNode
void SemanticAnalyser::Visit(RootNode *root) {
  for (auto &child : root->SemanticGetchildren()) {
    child->Accept(this);
  }
}

// Following are the nodes which should not be accessed directly
void SemanticAnalyser::Visit([[maybe_unused]] ExprNode *expr) {
  compiler_context_->ReportError("ExprNode is an abstract class and should not be visited directly");
}

void SemanticAnalyser::Visit([[maybe_unused]] StmtNode *stmt) {
  compiler_context_->ReportError("StmtNode is an abstract class and should not be visited directly");
}

void SemanticAnalyser::Visit([[maybe_unused]] DeclNode *decl) {
  compiler_context_->ReportError("DeclNode is an abstract class and should not be visited directly");
}

void SemanticAnalyser::Visit([[maybe_unused]] Initializer *initializer) {
  compiler_context_->ReportError("Initializer is an abstract class and should not be visited directly");
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
    compiler_context_->ReportError("function " + func_call->func_name_ + " is not defined");
    return;
  }

  // First visit all parameters
  for (auto &param : func_call->params_) {
    param->Accept(this);
  }
  // Then check the function definition and the passed parameters
  if (func_def->param_types_.size() != func_call->params_.size()) {
    compiler_context_->ReportError("function " + func_call->func_name_ + " expects " +
                                   std::to_string(func_def->param_types_.size()) + " parameters, but got " +
                                   std::to_string(func_call->params_.size()));
    return;
  }

  size_t param_count = func_def->param_types_.size();
  for (size_t i = 0; i < param_count; i++) {
    auto &def = func_def->param_types_[i];
    auto &param = func_call->params_[i];
    if (def != param->GetType()) {
      compiler_context_->ReportError("type mismatch for parameter " + std::to_string(i) + " of function " +
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
    compiler_context_->ReportError(LVal, "variable (" + LVal->ident_ + ") is not defined");
    return;
  }
  // This case is for circular reference in initializer, e.g., int a = a; int b[1] = {b[0]};
  // To be a little bit detailed,
  // Since once we finish the Visit of VarDefNode, we will set the variable true
  // if it is not true, which means the variable has not been processed through VarDefNode
  if (!var_def->is_being_defined_) {
    compiler_context_->ReportError(LVal,
                                   "variable (" + LVal->ident_ + ") is not defined yet, but be used as an initializer");
    return;
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
    compiler_context_->ReportError("Too many indices for variable " + LVal->ident_ + ": expected " +
                                   std::to_string(def_indices.size()) + ", but got " +
                                   std::to_string(LVal->indices_.size()));
  }

  // Analyze the indices if there are any
  bool index_is_const = true;
  for (auto &index : LVal->indices_) {
    index->Accept(this);
    if (index->GetType()->GetKind() != type::TypeKind::INT) {
      compiler_context_->ReportError("array subscript must be an integer");
    }
    if (!index->IsConst()) {
      // Since if there is no index,
      // the variable will not change the constant attribute
      index_is_const = false;
    }
  }

  // Then we can set the type of the LVal expression
  if (result_type != nullptr) {
    LVal->SetType(result_type);
  }
  LVal->SetType(var_def->type_);
  LVal->SetConst(var_def->is_const_ && index_is_const);

  /**
  * We can get the value of the LVal expression
  * If the LVal has following attributes:
  * 1. The variable is const
  * 2. All indices are constant expressions (if has)
  * 3. The type of the LVal expression is not a pointer type
  * which means the LVal expression is used in a function call

  * Then we can get the value from the symbol table directly.
  */
  // if (LVal->GetType()->GetKind() != type::TypeKind::PTR && LVal->IsConst()) {
  // }
}

// Then we can visit stmt nodes
void SemanticAnalyser::Visit(BlockStmtNode *block_stmt) {
  compiler_context_->GetSymbolTableContext()->EnterScope();
  for (auto &stmt : block_stmt->block_items_) {
    stmt->Accept(this);
  }
  compiler_context_->GetSymbolTableContext()->ExitScope();
}

void SemanticAnalyser::Visit(AssignStmtNode *assign_stmt) {
  // First visit the left and right expressions
  assign_stmt->lval_->Accept(this);
  assign_stmt->expr_->Accept(this);

  if (assign_stmt->lval_->GetType()->GetKind() == type::TypeKind::PTR ||
      assign_stmt->lval_->GetType()->GetKind() == type::TypeKind::UNKNOWN) {
    compiler_context_->ReportError(assign_stmt, "cannot assign to an array or pointer variable");
  }
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
void SemanticAnalyser::Visit(InitValNode *init_val) { init_val->value_->Accept(this); }

void SemanticAnalyser::Visit(InitListNode *init_list) {
  for (auto &init_val : init_list->init_values_) {
    init_val->Accept(this);
  }
}

void SemanticAnalyser::Visit(VarDefNode *var_def) {
  for (auto &index : var_def->indices_) {
    index->Accept(this);
    if (index->GetType()->GetKind() != type::TypeKind::INT) {
      compiler_context_->ReportError(var_def, "array size must be an integer");
    }
    if (!index->IsConst()) {
      compiler_context_->ReportError(var_def, "In definition, array size must be a constant expression");
    }
  }
  // Then we need to calcualte the size and store the dimensions(if has)
  auto entry = compiler_context_->GetSymbolTableContext()->LookupVar(var_def->ident_);
  if (entry == nullptr) {
    compiler_context_->ReportError(var_def, "variable " + var_def->ident_ + " is not defined");
    return;
  }

  if (!var_def->indices_.empty()) {
    std::vector<size_t> dimensions;
    size_t total_size = 1;
    for (const auto &index : var_def->indices_) {
      auto value = index->GetValue();
      auto int_value = value.GetIntValue();
      if (int_value <= 0) {
        compiler_context_->ReportError(var_def, "array size must be a positive integer");
      }
      // Then we can store the dimension size
      dimensions.push_back(static_cast<size_t>(int_value));
      total_size *= static_cast<size_t>(int_value);
    }
    // Reach here means the variable is an array variable, we need to set its type to array type
    auto element_type = entry->type_;
    auto array_type = compiler_context_->GetTypeContext()->GetArrayType(element_type, total_size);
    entry->type_ = array_type;
    entry->dimensions_ = dimensions;
  }

  if (var_def->init_values_ != nullptr) {
    var_def->init_values_->Accept(this);
    // Check whether initializing a const variable with a constant expression
    if (entry->is_const_ && !var_def->init_values_->IsConst()) {
      compiler_context_->ReportError(var_def, "const variable initializer must be a constant expression");
    }
    // Check whether the initializer is compatible with the variable type
    if (entry->dimensions_.empty() && !var_def->init_values_->IsScalar()) {
      compiler_context_->ReportError(var_def, "scalar variable cannot be initialized with a non-scalar initializer");
    }
    if (!entry->dimensions_.empty() && var_def->init_values_->IsScalar()) {
      compiler_context_->ReportError(var_def, "array variable cannot be initialized with a scalar initializer");
    }
  }
  // The circular reference will be checked in LValExprNode
  entry->is_being_defined_ = true;
}

void SemanticAnalyser::Visit(GlobalVarDeclNode *global_var_decl) {
  for (auto &var_def : global_var_decl->defs_) {
    auto success = compiler_context_->GetSymbolTableContext()->AddVar(var_def->ident_, global_var_decl->GetType(),
                                                                      global_var_decl->IsConst());
    if (!success) {
      compiler_context_->ReportError(var_def.get(),
                                     "variable " + var_def->ident_ + " is already defined in the current scope");
    }
    var_def->Accept(this);
  }
}

void SemanticAnalyser::Visit(LocalVarDeclNode *local_var_decl) {
  for (auto &var_def : local_var_decl->defs_) {
    auto success = compiler_context_->GetSymbolTableContext()->AddVar(var_def->ident_, local_var_decl->GetType(),
                                                                      local_var_decl->IsConst());
    if (!success) {
      compiler_context_->ReportError(var_def.get(),
                                     "variable " + var_def->ident_ + " is already defined in the current scope");
    }
    var_def->Accept(this);
  }
}

void SemanticAnalyser::Visit(FuncDefNode *func_def) {
  auto fparam_types = std::vector<type::Type *>{};
  for (auto &fparam : func_def->parameters_) {
    fparam->Accept(this);
    fparam_types.push_back(fparam->type_);
  }
  auto success =
      compiler_context_->GetSymbolTableContext()->AddFunc(func_def->func_name_, func_def->return_type_, fparam_types);
  if (!success) {
    compiler_context_->ReportError(func_def, "function " + func_def->func_name_ + " is already defined");
  }
  // Add paramaters to the symbol table context.
  auto func_entry = compiler_context_->GetSymbolTableContext()->LookupFunc(func_def->func_name_);
  for (auto &fparam : func_def->parameters_) {
    compiler_context_->GetSymbolTableContext()->AddFuncParam(func_entry, fparam->param_name_, fparam->type_);
  }

  // Then we can visit the function body
  if (func_def->body_ != nullptr) {
    func_def->body_->Accept(this);
  }
  compiler_context_->GetSymbolTableContext()->ResetCurrentFunc();
}

void SemanticAnalyser::Visit(FuncFParamNode *func_fparam) {
  const int dimension_count = func_fparam->indices_.size();
  for (int i = 0; i < dimension_count; i++) {
    auto &index = func_fparam->indices_[i];
    if (index == nullptr && i == 0) {
      continue;
    }
    if (index == nullptr) {
      compiler_context_->ReportError(func_fparam,
                                     "only the first dimension of an array parameter can be left unspecified");
      return;
    }
    index->Accept(this);
    if (index->GetType()->GetKind() != type::TypeKind::INT) {
      compiler_context_->ReportError(func_fparam, "array size must be an integer");
    }
    if (!index->IsConst()) {
      compiler_context_->ReportError(func_fparam,
                                     "In function parameter definition, array size must be a constant expression");
    }
  }
}

}  // namespace frontend::ast