#include "frontend/ast/visitor/semantic/sem_analysis.h"
#include <climits>
#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>
#include "ast/ast_base.h"
#include "ast/ast_decl.h"
#include "ast/ast_expr.h"
#include "ast/ast_stmt.h"
#include "ast/ast_utils.h"
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
      compiler_context_->ReportError(left, "invalid operand types for arithmetic operator: " +
                                               left->GetType()->ToString() + " and " + right->GetType()->ToString());
      return compiler_context_->GetTypeContext()->GetUnknownType();
    case OpCode::MOD:
      if ((lhs_is_int || lhs_is_bool) && (rhs_is_int || rhs_is_bool)) {
        return compiler_context_->GetTypeContext()->GetIntegerType();
      }
      compiler_context_->ReportError(left, "invalid operand types for modulo operator: " + left->GetType()->ToString() +
                                               " and " + right->GetType()->ToString());
      return compiler_context_->GetTypeContext()->GetUnknownType();

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
    compiler_context_->ReportError("Try to initialize non-array type with array initializers");
    return;
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
    // Since we only allow int OR float value, so if float has value,
    // then integer will definitely be a std::nullopt
    /**
     * @note: if original type is float, and the initializer is an integer, we need to do the convert.
     */
    if (is_float) {
      auto opt = init_node.GetIntValue();
      if (opt.has_value()) {
        auto int_value = opt.value();
        init_node.SetFloatValue(static_cast<float>(int_value));
      }
    }
    /**
     * @note: if original type is integer, and the initializer is a float, we need to do the convert.
     */
    if (is_int) {
      auto opt = init_node.GetFloatValue();
      if (opt.has_value()) {
        auto float_value = opt.value();
        init_node.SetIntValue(static_cast<int>(float_value));
      }
    }
  }
// #define Debug
#ifdef Debug

  debug_file_ << "Normalized initializer for variable " << entry->name_ << ":\n";
  for (auto &init_val : init->GetInitializers()) {
    if (init_val == nullptr) {
      debug_file_ << "nullptr\n";
    } else if (init_val->IsConst()) {
      auto &const_init = static_cast<initializer::ConstInitializer &>(*init_val);
      if (const_init.GetIntValue().has_value()) {
        debug_file_ << "int: " << const_init.GetIntValue().value() << "\n";
      } else if (const_init.GetFloatValue().has_value()) {
        debug_file_ << "float: " << const_init.GetFloatValue().value() << "\n";
      }
    } else {
      debug_file_ << "non-const initializer\n";
    }
  }
#endif
}

// override for constant scalar initializer
auto SemanticAnalyser::Normalize(symboltable::VarEntry *entry,
                                 std::unique_ptr<initializer::ConstInitializer> &init) -> void {
  auto type = entry->type_;
  bool is_int = type->GetKind() == type::TypeKind::INT;
  bool is_float = type->GetKind() == type::TypeKind::FLOAT;
  if (!is_int && !is_float) {
    compiler_context_->ReportError("Try to initialize non-scalar type with scalar initializers");
    return;
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
  } else if (init->IsConst()) {
    // Same as overrided `Normalize`
    /**
     * @note: it is doing type convert here
     * if original type is float, and the initializer is an integer, we need to do the convert.
     */
    if (is_float) {
      auto opt = init->GetIntValue();
      if (opt.has_value()) {
        auto int_value = opt.value();
        init->SetFloatValue(static_cast<float>(int_value));
      }
    }
    /**
     * if original type is integer, and the initializer is a float, we need to do the convert.
     */
    if (is_int) {
      auto opt = init->GetFloatValue();
      if (opt.has_value()) {
        auto float_value = opt.value();
        init->SetIntValue(static_cast<int>(float_value));
      }
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
    // After visiting the whole AST, we need to make sure that there is a function called main
    auto main_func = compiler_context_->GetSymbolTableContext()->LookupFunc("main");
    if (main_func == nullptr) {
      compiler_context_->Print("function main is not defined");
      compiler_context_->GetSymbolTableContext()->ResetCurrentFunc();
      compiler_context_ = nullptr;
      return 2;
    }
    auto main_return_type = main_func->return_type_;
    if (main_return_type->GetKind() == type::TypeKind::FLOAT) {
      compiler_context_->ReportWarning("main function should return an integer, but float is found");
    }
    if (main_return_type->GetKind() == type::TypeKind::VOID) {
      compiler_context_->ReportWarning("main function should return an integer, but void is found");
    }
    // Maybe we can allow parameters in main function, but we currently disable this feature.
    if (!main_func->param_types_.empty()) {
      compiler_context_->ReportWarning("main function should not have parameters, but " +
                                       std::to_string(main_func->param_types_.size()) + " are found");
    }
  } catch (const std::runtime_error &e) {
    // Has exception in semantic analysis, return 1 to indicate failure
    compiler_context_->Print(e.what());
    compiler_context_->GetSymbolTableContext()->ResetCurrentFunc();
    compiler_context_ = nullptr;
    return 1;
  } catch (const std::logic_error &e) {
    compiler_context_->Print(e.what());
    compiler_context_->GetSymbolTableContext()->ResetCurrentFunc();
    compiler_context_ = nullptr;
    return 2;
  } catch (...) {
    compiler_context_->Print("Unknown error occurred");
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

  auto all_const = binary_expr->left_->IsConst() && binary_expr->right_->IsConst();
  auto right_const_only = !binary_expr->left_->IsConst() && binary_expr->right_->IsConst();

  // Consider the case that one of the operands is a constant expression
  // Check the division by zero for the case that the right operand is a constant expression
  if (right_const_only) {
    // Report error while the right operand is zero and can be calculated in compiling
    if (binary_expr->operator_ == OpCode::DIV) {
      auto value = binary_expr->right_->GetValue();
      if (CheckZero(value)) {
        compiler_context_->ReportError(binary_expr->right_.get(), "division by zero");
        return;
      }
    }
    if (binary_expr->operator_ == OpCode::MOD) {
      auto value = binary_expr->right_->GetValue();
      // since we have checked the type in DecideResultType,
      // we are sure that the value must be an integer or a boolean.
      if (CheckZero(value)) {
        compiler_context_->ReportError(binary_expr->right_.get(), "modulo by zero");
        return;
      }
    }
  }

  // Then we need to calculate the value if both operands are constant
  if (all_const) {
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
    if (!CheckCast(def, param->GetType())) {
      compiler_context_->ReportError("type mismatch for parameter " + std::to_string(i) + " of function " +
                                     func_call->func_name_ + ": expected " + def->ToString() + ", but got " +
                                     param->GetType()->ToString());
      return;
    }
    if (def != param->GetType()) {
      compiler_context_->ReportWarning("Implicit type conversion for parameter " + std::to_string(i) + " of function " +
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

  // Since there are two different types of variables
  /**
   * @note: we use array to represent those variables which have to alloca memory
   * @note: we use pointer to represent those variables which represents as a pointer
   * @note: we deal with parameter variables as Pointer type.
   */
  if (!def_dims.empty() && LVal->indices_.size() == def_dims.size()) {
    if (var_def->type_->GetKind() == type::TypeKind::ARRAY) {
      // Deal with array definition
      auto array_type = static_cast<type::ArrayType *>(var_def->type_);
      result_type = array_type->GetElementType();
    } else if (var_def->type_->GetKind() == type::TypeKind::PTR) {
      // Deal with parameter variable, which is represented as pointer type
      auto ptr_type = static_cast<type::PtrType *>(var_def->type_);
      result_type = ptr_type->GetBaseType();
    } else {
      compiler_context_->ReportError(LVal, "variable (" + LVal->ident_ + ") is not an array variable, but has indices");
      return;
    }
  } else if (!def_dims.empty() && LVal->indices_.size() < def_dims.size()) {
    TypePtr element_type = nullptr;
    if (var_def->type_->GetKind() == type::TypeKind::ARRAY) {
      // means a variable defined
      element_type = static_cast<type::ArrayType *>(var_def->type_)->GetElementType();
    } else if (var_def->type_->GetKind() == type::TypeKind::PTR) {
      // means a parameter variable, which is represented as pointer type
      element_type = static_cast<type::PtrType *>(var_def->type_)->GetBaseType();
    } else {
      compiler_context_->ReportError(LVal, "variable (" + LVal->ident_ + ") is not an array variable, but has indices");
      return;
    }
    result_type = compiler_context_->GetTypeContext()->GetPtrType(element_type);
  } else if (!def_dims.empty() && LVal->indices_.size() > def_dims.size()) {
    compiler_context_->ReportError(LVal, "too many indices for array variable (" + LVal->ident_ + ")");
    return;
  }
  /**
   * @note: we should allow LVal to be pointer type
   * since it can be passed as a parameter to a function
   */

  // Analyze the indices if there are any
  bool index_is_const = true;
  for (auto &index : LVal->indices_) {
    index->Accept(this);
    if (index->GetType()->GetKind() != type::TypeKind::INT && index->GetType()->GetKind() != type::TypeKind::BOOL) {
      compiler_context_->ReportError(index.get(), "array subscript must be an integer or boolean");
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
  LVal->symbol_id_ = var_def->symbol_id_;
  /**
  * We can get the value of the LVal expression
  * If the LVal has following attributes:
  * 1. The variable is const
  * 2. All indices are constant expressions (if has)
  * 3. The type of the LVal expression is not a pointer type
  * which means the LVal expression is used in a function call

  * Then we can get the value from the symbol table directly.
  */
  if (LVal->IsConst() && LVal->GetType()->GetKind() != type::TypeKind::PTR) {
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

  if (assign_stmt->lval_->GetType()->GetKind() == type::TypeKind::VOID ||
      assign_stmt->lval_->GetType()->GetKind() == type::TypeKind::UNKNOWN) {
    compiler_context_->ReportError(assign_stmt, "cannot assign to a void or unknown variable");
    return;
  }
  /**
   * @cond: left value is a constant variable, which means the assigned value is a constant expression
   */
  if (assign_stmt->lval_->IsConst()) {
    // we cannot assign a value to a constant variable
    compiler_context_->ReportError(assign_stmt->lval_.get(), "cannot assign to a constant variable");
    return;
  }
  /**
   * @cond: left value is a pointer
   * since we do not support pointer assignment, we should report error
   */
  if (assign_stmt->lval_->GetType()->GetKind() == type::TypeKind::PTR) {
    compiler_context_->ReportError(assign_stmt, "cannot assign to a pointer variable");
    return;
  }

  /**
   * @cond: right value cannot be a pointer or an unknown type or void
   */
  if (assign_stmt->expr_->GetType()->GetKind() == type::TypeKind::PTR ||
      assign_stmt->expr_->GetType()->GetKind() == type::TypeKind::UNKNOWN ||
      assign_stmt->expr_->GetType()->GetKind() == type::TypeKind::VOID) {
    compiler_context_->ReportError(assign_stmt, "cannot assign a pointer or unknown type or void value to a variable");
    return;
  }
}

void SemanticAnalyser::Visit(ExprStmtNode *expr_stmt) {
  if (expr_stmt->expr_ != nullptr) {
    expr_stmt->expr_->Accept(this);
  }
}

void SemanticAnalyser::Visit(IfStmtNode *if_stmt) {
  if_stmt->condition_->Accept(this);
  // Assure condition is not ptr or unknown type
  if (if_stmt->condition_->GetType()->GetKind() == type::TypeKind::UNKNOWN) {
    compiler_context_->ReportError(if_stmt->condition_.get(), "condition expression has unknown type");
    return;
  }
  if (if_stmt->condition_->GetType()->GetKind() == type::TypeKind::PTR) {
    compiler_context_->ReportError(if_stmt->condition_.get(), "condition expression cannot be a pointer");
    return;
  }
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
  auto current_func = compiler_context_->GetSymbolTableContext()->GetCurrentFunc();
  if (current_func == nullptr) {
    compiler_context_->ReportError(return_stmt, "return statement is not inside a function");
    return;
  }
  auto return_type = current_func->return_type_;
  if (return_stmt->expr_ != nullptr) {
    return_stmt->expr_->Accept(this);
    // if return type is void, we cannot return a value
    // or the returned value is a pointer type, which is not allowed
    if (return_type->GetKind() == type::TypeKind::VOID ||
        return_stmt->expr_->GetType()->GetKind() == type::TypeKind::PTR) {
      compiler_context_->ReportError(return_stmt, "return type mismatch: expected " + return_type->ToString() +
                                                      ", but got " + return_stmt->expr_->GetType()->ToString());
      return;
    }
    return;
  }
  // The true return is void
  // make sure required return type is void
  if (return_type->GetKind() != type::TypeKind::VOID) {
    compiler_context_->ReportError(return_stmt,
                                   "return type mismatch: expected " + return_type->ToString() + ", but got void");
    return;
  }
}

// Then Declaration nodes
void SemanticAnalyser::Visit(InitValNode *init_val) {
  init_val->value_->Accept(this);
  if (init_val->value_->GetType()->GetKind() == type::TypeKind::UNKNOWN) {
    compiler_context_->ReportError(init_val, "initializer expression has unknown type");
    return;
  }
  if (init_val->value_->GetType()->GetKind() == type::TypeKind::PTR) {
    compiler_context_->ReportError(init_val, "initializer expression cannot be a pointer");
    return;
  }
  if (init_val->value_->GetType()->GetKind() == type::TypeKind::VOID) {
    compiler_context_->ReportError(init_val, "initializer expression cannot be void");
    return;
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
    if (index->GetType()->GetKind() != type::TypeKind::INT && index->GetType()->GetKind() != type::TypeKind::BOOL) {
      auto msg = "array size must be an integer or boolean, but got " + index->GetType()->ToString();
      compiler_context_->ReportError(var_def, msg);
      return;
    }
    if (!index->IsConst()) {
      compiler_context_->ReportError(var_def, "In definition, array size must be a constant expression");
      return;
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
      if (int_value < 0) {
        compiler_context_->ReportError(var_def, "array size cannot be negative");
        return;
      }
      if (int_value == 0) {
        compiler_context_->ReportError(var_def, "array size cannot be zero");
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
      return;
    }
    // Check whether the initializer is compatible with the variable type
    if (entry->dimensions_.empty() && !var_def->init_values_->IsScalar()) {
      compiler_context_->ReportError(var_def, "scalar variable cannot be initialized with a non-scalar initializer");
      return;
    }
    if (!entry->dimensions_.empty() && var_def->init_values_->IsScalar()) {
      compiler_context_->ReportError(var_def, "array variable cannot be initialized with a scalar initializer");
      return;
    }
  }
  // Set the symbol_id for vardef node
  var_def->symbol_id_ = entry->symbol_id_;

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
      return;
    }
    // means the variable is a const scalar variable, we can set its value in the symbol table entry
    if (var_def->init_values_ == nullptr) {
      compiler_context_->ReportError(var_def, "const scalar variable must be initialized");
      return;
    }
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
      return;
    }
    Normalize(entry, const_value);
    entry->initializer_ = std::move(const_value);
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
      return;
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
      return;
    }
    var_def->Accept(this);
  }
}

void SemanticAnalyser::Visit(FuncDefNode *func_def) {
  auto fparam_types = std::vector<type::Type *>{};
  for (auto &fparam : func_def->parameters_) {
    fparam->Accept(this);
    fparam_types.emplace_back(fparam->type_);
  }
  auto success =
      compiler_context_->GetSymbolTableContext()->AddFunc(func_def->func_name_, func_def->return_type_, fparam_types);
  if (!success) {
    compiler_context_->ReportError(func_def, "function " + func_def->func_name_ + " is already defined");
    return;
  }
  // Add paramaters to the symbol table context.
  auto func_entry = compiler_context_->GetSymbolTableContext()->LookupFunc(func_def->func_name_);
  for (auto &fparam : func_def->parameters_) {
    // Assume there is dimension
    auto dimensions = std::vector<int>{};
    if (fparam->type_->GetKind() == type::TypeKind::PTR) {
      // if it is ptr , we need to record the dimensions
      for (const auto &index : fparam->indices_) {
        if (index == nullptr) {
          // we indicate the first dimension by assign it with -1, since the first dimension can be left unspecified
          dimensions.push_back(-1);
          continue;
        }
        if (!index->IsConst()) {
          compiler_context_->ReportError(fparam.get(),
                                         "In function parameter definition, array size must be a constant expression");
          return;
        }
        if (index != nullptr) {
          auto value = index->GetValue();
          auto int_value = value.GetIntValue();
          if (int_value <= 0) {
            compiler_context_->ReportError(fparam.get(), "array size must be a positive integer");
            return;
          }
          dimensions.push_back(int_value);
        }
      }
    }
    compiler_context_->GetSymbolTableContext()->AddFuncParam(func_entry, fparam->param_name_, fparam->type_,
                                                             dimensions);
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
    if (index->GetType()->GetKind() != type::TypeKind::INT && index->GetType()->GetKind() != type::TypeKind::BOOL) {
      compiler_context_->ReportError(func_fparam, "array size must be an integer or boolean");
      return;
    }
    if (!index->IsConst()) {
      compiler_context_->ReportError(func_fparam,
                                     "In function parameter definition, array size must be a constant expression");
      return;
    }
  }
  /**
   * @note we have set it to the true type during parser phase.
   */
}

}  // namespace frontend::ast