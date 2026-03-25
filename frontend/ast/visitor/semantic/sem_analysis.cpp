#include "frontend/ast/visitor/semantic/sem_analysis.h"
#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>
#include "ast/ast_base.h"
#include "ast/ast_decl.h"
#include "ast/ast_expr.h"
#include "ast/ast_stmt.h"
#include "ast/value/value.h"
#include "ast/visitor/semantic/calculate.h"
#include "utils/flatten/astinit.h"
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

/**
 * @brief Helper function to calculate the linear index of an array element
 * Base on the dimensions and the indices of the array element.
 * @example:
 * For instance, const int a[10] = {1,2,3};
 * in the initializer list, the value initializer will be {1,2,3,nullptr,...}
 * then if we want to get the value of [2], we can calculate the linear index as follows:
 * linear index = 2(index[0]) * dim[0] + 0 * dim[1] + ...
 */
auto SemanticAnalyser::Linear(const std::vector<int> &dimensions, const std::vector<int> &indices) -> int {
  if (dimensions.size() != indices.size()) {
    throw std::logic_error("Dimensions size and indices size do not match");
  }
  int linear_index = 0;
  int dim_size = 1;
  for (int i = dimensions.size() - 1; i >= 0; i--) {
    linear_index += indices[i] * dim_size;
    dim_size *= dimensions[i];
  }
  return linear_index;
}

/**
 * @brief Normalize function will fill the default value, which was initially set as nullptr,
 * and do the cast if needed
 */
// override for constant list initializer
auto SemanticAnalyser::Normalize(symboltable::VarEntry *entry,
                                 std::unique_ptr<initializer::ArrayInitializer> &init) -> void {
  // First check the type
  auto type = entry->type_;
  // The initialized variable must be an array variable, otherwise it is a logic error
  if (type->GetKind() != type::TypeKind::ARRAY) {
    throw std::logic_error("Try to initialize non-array type with list initializers");
  }
  auto array_type = static_cast<type::ArrayType *>(type);
  auto element_type = array_type->GetElementType();
  auto is_int = element_type->GetKind() == type::TypeKind::INT;
  auto is_float = element_type->GetKind() == type::TypeKind::FLOAT;
  for (auto &init_val : init->GetUpdateInitializers()) {
    if (init_val == nullptr) {
      if (is_int) {
        auto new_init = std::make_unique<initializer::ConstInitializer>();
        new_init->SetIntValue(0);
        init_val = std::move(new_init);
      }
      if (is_float) {
        auto new_init = std::make_unique<initializer::ConstInitializer>();
        new_init->SetFloatValue(0.0F);
        init_val = std::move(new_init);
      }
      continue;
    }
    if (!init_val->IsScalar()) {
      compiler_context_->ReportError(
          "initializer for array element which has been flattened must be a scalar expression");
      return;
    }
    if (!init_val->IsConst()) {
      compiler_context_->ReportError(
          "initializer for array element which has been flattened must be a constant expression");
      return;
    }

    auto &init_node = static_cast<initializer::ConstInitializer &>(*init_val);
    if (is_int && init_node.GetIntValue() == std::nullopt) {
      auto float_value = init_node.GetFloatValue().value();
      init_node.SetIntValue(static_cast<int>(float_value));
    }
    if (is_float && init_node.GetFloatValue() == std::nullopt) {
      auto int_value = init_node.GetIntValue().value();
      init_node.SetFloatValue(static_cast<float>(int_value));
    }
  }
}

// override for constant scalar initializer
auto SemanticAnalyser::Normalize(symboltable::VarEntry *entry,
                                 std::unique_ptr<initializer::ConstInitializer> &init) -> void {
  auto type = entry->type_;
  bool is_int = type->GetKind() == type::TypeKind::INT;
  bool is_float = type->GetKind() == type::TypeKind::FLOAT;
  if (!is_int && !is_float) {
    throw std::logic_error("Try to initialize non-scalar type with scalar initializers");
  }
  if (init == nullptr) {
    /**  Here might cause memory leak
     * since the unique_ptr originally manage a pointer which is a `nullptr`
     * we can safely just reset the unique_ptr to a new pointer without worrying about the memory leak
     */
    init = std::make_unique<initializer::ConstInitializer>();
    if (is_int) {
      init->SetIntValue(0);
    }
    if (is_float) {
      init->SetFloatValue(0.0F);
    }
  }
  if (init->IsConst()) {
    if (is_int && init->GetIntValue() == std::nullopt) {
      auto float_value = init->GetFloatValue().value();
      init->SetIntValue(static_cast<int>(float_value));
    }
    if (is_float && init->GetFloatValue() == std::nullopt) {
      auto int_value = init->GetIntValue().value();
      init->SetFloatValue(static_cast<float>(int_value));
    }
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
    compiler_context_->ReportError(e.what());
    compiler_context_->GetSymbolTableContext()->ResetCurrentFunc();
    compiler_context_ = nullptr;
    return 1;
  } catch (const std::logic_error &e) {
    compiler_context_->ReportError(e.what());
    compiler_context_->GetSymbolTableContext()->ResetCurrentFunc();
    compiler_context_ = nullptr;
    return 2;
  } catch (...) {
    compiler_context_->ReportError("Unknown error occurred");
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
  auto def_dims = var_def->dimensions_;
  TypePtr result_type = nullptr;

  if (!def_dims.empty() && LVal->indices_.size() == def_dims.size()) {
    assert(var_def->type_->GetKind() == type::TypeKind::ARRAY);
    result_type = static_cast<type::ArrayType *>(var_def->type_)->GetElementType();
  }
  if (!def_dims.empty() && LVal->indices_.size() < def_dims.size()) {
    assert(var_def->type_->GetKind() == type::TypeKind::ARRAY);
    auto element_type = static_cast<type::ArrayType *>(var_def->type_)->GetElementType();
    result_type = compiler_context_->GetTypeContext()->GetPtrType(element_type);
  }
  if (def_dims.size() > LVal->indices_.size()) {
    compiler_context_->ReportError("Too many indices for variable " + LVal->ident_ + ": expected " +
                                   std::to_string(def_dims.size()) + ", but got " +
                                   std::to_string(LVal->indices_.size()));
  }

  // Analyze the indices if there are any
  bool index_is_const = true;
  for (auto &index : LVal->indices_) {
    index->Accept(this);
    if (index->GetType()->GetKind() != type::TypeKind::INT) {
      compiler_context_->ReportError("array subscript must be an integer");
      return;
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
  } else {
    LVal->SetType(var_def->type_);
  }
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
  // TODO(loki): Get initializer for constant variable
  if (LVal->IsConst()) {
    // If it is a constant variable
    // Calculate the linear index if it is an array variable
    if (!def_dims.empty()) {
      std::vector<int> indices;
      // Get the indices value and check whether it is out of bounds or not
      for (const auto &index : LVal->indices_) {
        auto value = index->GetValue();
        /**
         * @note: Perhaps we can cast the float to integer, however, it is not allowed now.
         */
        auto int_value = value.GetIntValue();
        if (int_value < 0) {
          compiler_context_->ReportError(LVal, "array subscript cannot be negative");
          return;
        }
        indices.push_back(int_value);
      }
      int linear_index = Linear(def_dims, indices);
      auto array_initializer = static_cast<initializer::ArrayInitializer *>(var_def->initializer_.get());
      if (array_initializer == nullptr) {
        compiler_context_->ReportError(LVal, "initializer for array variable is not an array initializer");
        return;
      }
      auto &init_values = array_initializer->GetInitializers();
      // If the linear index is out of bounds, it is normally impossible
      if (static_cast<size_t>(linear_index) >= init_values.size()) {
        compiler_context_->ReportError(LVal, "array subscript out of bounds");
        return;
      }
      // We can get the initializer of the array element now
      auto initializer = init_values[linear_index].get();
      if (!initializer->IsScalar()) {
        compiler_context_->ReportError(
            LVal, "initializer for array element which has been flattened must be a scalar expression");
        return;
      }
      if (!initializer->IsConst()) {
        compiler_context_->ReportError(
            LVal, "initializer for array element which has been flattened must be a constant expression");
        return;
      }
      auto const_initializer = static_cast<initializer::ConstInitializer *>(initializer);
      LVal->GetValue() = const_initializer->GetValue();
    }
    // it is scalar variable, we can get the value directly
    else {
      auto initializer = var_def->initializer_.get();
      // Do some errors handling first
      if (initializer == nullptr) {
        compiler_context_->ReportError(LVal, "initializer for scalar variable is null");
        return;
      }
      if (!initializer->IsScalar()) {
        compiler_context_->ReportError(LVal, "initializer for scalar variable must be a scalar expression");
        return;
      }
      if (!initializer->IsConst()) {
        compiler_context_->ReportError(LVal, "initializer for scalar variable must be a constant expression");
        return;
      }

      // Then we can get the value from the initializer
      auto const_initializer = static_cast<initializer::ConstInitializer *>(initializer);
      LVal->GetValue() = const_initializer->GetValue();
    }
  }
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
void SemanticAnalyser::Visit(InitValNode *init_val) {
  init_val->value_->Accept(this);
  if (init_val->value_->GetType()->GetKind() == type::TypeKind::UNKNOWN) {
    compiler_context_->ReportError(init_val, "initializer expression has unknown type");
  }
  if (init_val->value_->GetType()->GetKind() == type::TypeKind::PTR) {
    compiler_context_->ReportError(init_val, "initializer expression cannot be a pointer");
  }
  // since the init_val->value_ is an expression
  // it will only be estimated as a `UNKNOWN` type or a `PTR` type which is not valid
  // so we don't need to check other cases such as `ARRAY` type
}

void SemanticAnalyser::Visit(InitListNode *init_list) {
  for (auto &init_val : init_list->init_values_) {
    init_val->Accept(this);
  }
}

void SemanticAnalyser::Visit(VarDefNode *var_def) {
  for (auto &index : var_def->indices_) {
    index->Accept(this);
    if (index->GetType()->GetKind() != type::TypeKind::INT) {
      auto msg = "array size must be an integer, but got " + index->GetType()->ToString();
      compiler_context_->ReportError(var_def, msg);
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
    std::vector<int> dimensions;
    size_t total_size = 1;
    for (const auto &index : var_def->indices_) {
      auto value = index->GetValue();
      auto int_value = value.GetIntValue();
      if (int_value <= 0) {
        compiler_context_->ReportError(var_def, "array size must be a positive integer");
        return;
      }
      // Then we can store the dimension size
      dimensions.push_back(int_value);
      total_size *= int_value;
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
  // If the variable is constant and init_values is all constant or is null
  // then we can set the value of the variable in the symbol table entry

  /**
   * @cond: if it is a constant variable and need to be flattened,
   * @then: we will flatten the initializer list and set the value in the symbol table entry
   */
  if (entry->is_const_ && !entry->dimensions_.empty()) {
    // means the variable is const, we can set its value in the symbol table entry
    auto result =
        flatten_helper_.FlattenInitializer(var_def->init_values_.get(), entry->dimensions_, initializer::HandleInAst);
    // result is the flattened initializer
    auto result_ptr = std::make_unique<initializer::ArrayInitializer>(std::move(result));
    // Then we need to normalize the initializer
    // First put the result into the RAII wrapper
    Normalize(entry, result_ptr);
    entry->initializer_ = std::move(result_ptr);

  }
  /**
   * @cond: if the entry is not a constant variable but is an array variable,
   * @then: we will check whether the initializer list is overflow or not.
   */
  else if (!entry->is_const_ && !entry->dimensions_.empty()) {
    /**
     * @brief If the variable is initialize by default,
     * it will never cause the overflow, so we don't need to check
     */
    if (var_def->init_values_ != nullptr) {
      auto result =
          flatten_helper_.FlattenInitializer(var_def->init_values_ == nullptr ? nullptr : var_def->init_values_.get(),
                                             entry->dimensions_, initializer::CheckInAst);
    }
  }
  /**
   * @brief: if the variable is a scalar variable
   */
  else if (entry->is_const_ && entry->dimensions_.empty()) {
    if (var_def->init_values_ == nullptr) {
      compiler_context_->ReportError(var_def, "const scalar variable must be initialized");
    } else {
      // means the variable is a const scalar variable, we can set its value in the symbol table entry
      if (var_def->init_values_ == nullptr) {
        compiler_context_->ReportError(var_def, "const scalar variable must be initialized");
      } else {
        auto init_node = static_cast<frontend::ast::InitValNode *>(var_def->init_values_.get());
        auto value = init_node->value_->GetValue();
        auto const_value = std::make_unique<initializer::ConstInitializer>();
        if (value.GetType() == ValueType::INT || value.GetType() == ValueType::BOOL) {
          const_value->SetIntValue(value.GetIntValue());
        } else if (value.GetType() == ValueType::FLOAT) {
          const_value->SetFloatValue(value.GetFloatValue());
        } else {
          compiler_context_->ReportError(
              var_def, "const scalar variable can only be initialized with an integer or float constant expression");
        }
        Normalize(entry, const_value);
        entry->initializer_ = std::move(const_value);
      }
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