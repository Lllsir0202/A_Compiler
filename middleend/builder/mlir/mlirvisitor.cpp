#include "middleend/builder/mlir/mlirvisitor.h"
#include <cstddef>
#include "ast/ast_expr.h"
#include "ast/ast_utils.h"
#include "frontend/ast/ast_spec.h"
#include "middleend/mlir/dialect/arithm/arithm.h"
#include "middleend/mlir/dialect/func/func.h"
#include "middleend/mlir/dialect/var/var.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/region.h"
#include "mlir/dialect/arithm/unaryop.h"
#include "mlir/utils/struct/block.h"
#include "mlir/utils/value/value.h"
#include "type/type.h"
#include "utils/flatten/mlirinit.h"
#define GET_MLIR_CONTEXT() compiler_context_->GetIRContext()->GetMLIRContext()
#define GET_TYPE_CONTEXT() compiler_context_->GetTypeContext()
#define GET_SYMBOLTABLE_CONTEXT() compiler_context_->GetSymbolTableContext()
namespace middleend {

#include "middleend/builder/mlir/binary_handler.def"

/**
 * @brief The Helper functions for handing out operators
 */
inline auto MLIRVisitor::HandleBinaryOperator(mlir::Value *lhs, mlir::Value *rhs,
                                              frontend::ast::BinaryExprNode *binary_expr,
                                              context::CompilerContext *compiler_context) -> mlir::Value * {
  mlir::Value *result = nullptr;
  switch (binary_expr->operator_) {
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
  return result;
}

#include "middleend/builder/mlir/unary_handler.def"

inline auto MLIRVisitor::HandleUnaryOperator(mlir::Value *operand, frontend::ast::UnaryExprNode *unary_expr,
                                             context::CompilerContext *compiler_context) -> mlir::Value * {
  /**
   * @brief Since the unary positive is the same as doing nothing,
   * we will just do nothing when we encounter a unary positive operator
   */
  mlir::Value *result = nullptr;
  switch (unary_expr->operator_) {
    UNARY_OPERATOR_HANDLER(NOT, Not)
    UNARY_OPERATOR_HANDLER(NEGATIVE, Neg)
    default:
      compiler_context->ReportError("Unsupported unary operator.");
  }
  return result;
}

inline auto MLIRVisitor::HandleLValExpr(frontend::ast::LValExprNode *lval_expr, bool get_ptr) -> mlir::Value * {
  /**
   * @brief The lval expression shall be a variable
   */
  auto symbol_id = lval_expr->symbol_id_;
  auto entry = GET_SYMBOLTABLE_CONTEXT()->GetVarEntryById(symbol_id);
  if (entry == nullptr) {
    compiler_context_->ReportError(lval_expr, "variable is not defined");
    return nullptr;
  }
  // Since the variable is defined, we can get its value from the symbol table entry
  auto ptr = GET_MLIR_CONTEXT()->GetResourceContext()->GetValueFromVar(entry);
  mlir::var::VarGetPtrOp *var_get_op = nullptr;
  // if the variable is a array
  if (entry->dimensions_.empty()) {
    // Not an array variable, we can directly get the value from the symbol table entry
    var_get_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarGetPtrOp>(GET_MLIR_CONTEXT(), ptr);
    GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(var_get_op);
    GET_MLIR_CONTEXT()->GetResourceContext()->MapExprToValue(lval_expr, var_get_op->GetRes());
    auto type = entry->type_;
    /* Add use and set operation which generates the result */
    ptr->AddUse(var_get_op, 0);

    var_get_op->GetRes()->SetType(type);
  } else {
    // Is an array
    auto indices = std::vector<mlir::Value *>{};
    for (const auto &index : lval_expr->indices_) {
      index->Accept(this);
      auto index_value = GET_MLIR_CONTEXT()->GetResourceContext()->GetValueFromExpr(index.get());
      indices.emplace_back(index_value);
    }
    var_get_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarGetPtrOp>(GET_MLIR_CONTEXT(), ptr);
    GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(var_get_op);
    GET_MLIR_CONTEXT()->GetResourceContext()->MapExprToValue(lval_expr, var_get_op->GetRes());
    // The type of the lval expression has been calculated in the semantic analysis phase.
    auto type = lval_expr->GetType();
    var_get_op->GetRes()->SetType(type);
    /* Add use and set operation which generates the result */
    ptr->AddUse(var_get_op, 0);
    int i = 1;
    for (const auto &index_value : indices) {
      index_value->AddUse(var_get_op, i++);
    }
  }
  if (get_ptr) {
    GET_MLIR_CONTEXT()->GetResourceContext()->MapExprToValue(lval_expr, var_get_op->GetRes());
    return var_get_op->GetRes();
  }
  // If we want to get the value of the variable,
  // we need to create a var.get operation to get the value from the pointer
  auto var_get_value_op =
      GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarGetValOp>(GET_MLIR_CONTEXT(), var_get_op->GetRes());
  GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(var_get_value_op);
  GET_MLIR_CONTEXT()->GetResourceContext()->MapExprToValue(lval_expr, var_get_value_op->GetRes());
  var_get_op->GetRes()->AddUse(var_get_value_op, 0);
  // Since we will never get the value of an array(or pointer) type LVal expression,
  // the type of the var.get operation should be the same as the type of the lval expression,
  // which has been calculated in the semantic analysis phase.
  // In other words, getpointer shall only be used while:
  // 1. Assign
  // 2. Function call with array(or pointer) type parameters
  var_get_value_op->GetRes()->SetType(lval_expr->GetType());
  return var_get_value_op->GetRes();
}
// TODO(loki): implement the MLIRVisitor::Visitor functions to generate MLIR IR
/**
 * @brief This file is the implementation of the MLIRVisitor class,
 * which will be used to traverse the AST and generate the MLIR IR.
 */

// RootNode
void MLIRVisitor::Visit(frontend::ast::RootNode *root) {
  // Create the module operation, which is the top-level operation in MLIR
  auto module_op = GET_MLIR_CONTEXT()->CreateOperation<middleend::mlir::ModuleOp>();
  GET_MLIR_CONTEXT()->SetInsertPoint(module_op->regions_[0]->blocks_[0]);
  GET_MLIR_CONTEXT()->SetCurrentRegion(module_op->regions_[0]);
  /**
   * @brief Visit all children
   */
  for (const auto &child : root->GetChildren()) {
    child->Accept(this);
  }
}

// Should not be called directly, since they are abstract classes
void MLIRVisitor::Visit([[maybe_unused]] frontend::ast::ExprNode *expr) {
  compiler_context_->ReportError("ExprNode: Should not visit an abstract expression node directly.");
}

void MLIRVisitor::Visit([[maybe_unused]] frontend::ast::StmtNode *stmt) {
  compiler_context_->ReportError("StmtNode: Should not visit an abstract statement node directly.");
}

void MLIRVisitor::Visit([[maybe_unused]] frontend::ast::Initializer *initializer) {
  compiler_context_->ReportError("Initializer: Should not visit an abstract initializer node directly.");
}

void MLIRVisitor::Visit([[maybe_unused]] frontend::ast::DeclNode *decl) {
  compiler_context_->ReportError("DeclNode: Should not visit an abstract declaration node directly.");
}

// Expression nodes
void MLIRVisitor::Visit(frontend::ast::BinaryExprNode *binary_expr) {
  auto lhs = binary_expr->left_.get();
  lhs->Accept(this);
  auto rhs = binary_expr->right_.get();
  rhs->Accept(this);

  /* Get the type of the binary expression */
  auto type = binary_expr->GetType();

  auto lhs_value = GET_MLIR_CONTEXT()->GetResourceContext()->GetValueFromExpr(lhs);
  auto rhs_value = GET_MLIR_CONTEXT()->GetResourceContext()->GetValueFromExpr(rhs);
  // Do cast to ensure the type of the left and right operands,
  // are the same with the type we decided in the semantic analysis phase.
  if (lhs->GetType() != type) {
    auto cast_op =
        GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::CastOp>(GET_MLIR_CONTEXT(), lhs_value, lhs->GetType(), type);
    GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(cast_op);
    lhs_value->AddUse(cast_op, 0);
    lhs_value = cast_op->GetRes();
  }
  if (rhs->GetType() != type) {
    auto cast_op =
        GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::CastOp>(GET_MLIR_CONTEXT(), rhs_value, rhs->GetType(), type);
    GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(cast_op);
    rhs_value->AddUse(cast_op, 0);
    rhs_value = cast_op->GetRes();
  }

  /**
   * @brief we use the map to get value from the ExprNode*,
   * since every expression node will ONLY generate one value,
   * so we can just use the expression node as the key to get the value from the map, and then we can use the value to
   * build the operation for this binary expression
   */
  auto res = HandleBinaryOperator(lhs_value, rhs_value, binary_expr, compiler_context_);
  res->SetType(type);
  GET_MLIR_CONTEXT()->GetResourceContext()->MapExprToValue(binary_expr, res);
}
void MLIRVisitor::Visit(frontend::ast::UnaryExprNode *unary_expr) {
  auto operand = unary_expr->operand_.get();
  operand->Accept(this);

  auto operand_value = GET_MLIR_CONTEXT()->GetResourceContext()->GetValueFromExpr(operand);
  auto type = unary_expr->GetType();

  // Do cast to ensure the type of the operand,
  if (type != operand->GetType()) {
    auto cast_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::CastOp>(GET_MLIR_CONTEXT(), operand_value,
                                                                             operand->GetType(), type);
    GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(cast_op);
    operand_value->AddUse(cast_op, 0);
    operand_value = cast_op->GetRes();
  }

  /**
   * @brief the operand has been visited,
   * and the result of the operand shall be stored in the MLIR StateContext,
   * also it should be the top value */
  auto res = HandleUnaryOperator(operand_value, unary_expr, compiler_context_);
  res->SetType(type);
  GET_MLIR_CONTEXT()->GetResourceContext()->MapExprToValue(unary_expr, res);
}

void MLIRVisitor::Visit(frontend::ast::FuncCallExprNode *func_call) {
  auto func_name = func_call->func_name_;
  std::vector<mlir::Value *> params;
  /**
   * @brief Since the func.call is similar to the funcdef,
   * but note that:
   * @note: if the param is a pointer(in parser, we regard the parameter with dimensions as a pointer),
   * we cannot directly get the value of the parameter, since the value of the parameter is actually a pointer
   * we need to get the pointer of the parameter, and then we can use the pointer to build the func.call operation
   */
  auto func_def_op = GET_MLIR_CONTEXT()->GetCurrentFuncDefOp();
  auto defined_types = func_def_op->GetArguments();
  for (size_t i = 0; i < func_call->params_.size(); ++i) {
    auto param = func_call->params_[i].get();
    auto type = defined_types[i]->GetType();
    if (param->GetType()->GetKind() == type::TypeKind::PTR) {
      // Since if the param is ptr and is not the same as the defined type,
      // it must be a mistake, so we do not need to worry about the cast.
      auto param_value = HandleLValExpr(static_cast<frontend::ast::LValExprNode *>(param), true);
      params.emplace_back(param_value);
    } else {
      param->Accept(this);
      auto param_value = GET_MLIR_CONTEXT()->GetResourceContext()->GetValueFromExpr(param);

      if (param->GetType() != type) {
        // Need to do cast to ensure the type of the parameter
        auto param_value = GET_MLIR_CONTEXT()->GetResourceContext()->GetValueFromExpr(param);
        auto cast_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::CastOp>(GET_MLIR_CONTEXT(), param_value,
                                                                                 param->GetType(), type);
        param_value->AddUse(cast_op, 0);
        GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(cast_op);
        param_value = cast_op->GetRes();
      }
      params.emplace_back(param_value);
    }
  }
  auto return_type = func_call->GetType();
  auto func_call_op = GET_MLIR_CONTEXT()->CreateOperation<middleend::mlir::func::FuncCallOp>(
      GET_MLIR_CONTEXT(), func_name, params, return_type);
  GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(func_call_op);
  // Add use for the arguments
  for (size_t i = 0; i < params.size(); ++i) {
    params[i]->AddUse(func_call_op, i);
  }
}

void MLIRVisitor::Visit(frontend::ast::LValExprNode *lval_expr) {
  auto res = HandleLValExpr(lval_expr, false);
  if (res == nullptr) {
    compiler_context_->ReportError(lval_expr, "Failed to handle LVal expression.");
    return;
  }
}

void MLIRVisitor::Visit(frontend::ast::IntLiteralExprNode *int_literal) {
  auto const_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::ConstOp>(GET_MLIR_CONTEXT(), int_literal->value_);
  GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(const_op);
  const_op->GetResult()->SetType(GET_TYPE_CONTEXT()->GetIntegerType());
  // Map the result of the const operation with the int literal ExprNode*
  GET_MLIR_CONTEXT()->GetResourceContext()->MapExprToValue(int_literal, const_op->GetResult());
}

void MLIRVisitor::Visit(frontend::ast::FloatLiteralExprNode *float_literal) {
  auto const_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::ConstOp>(GET_MLIR_CONTEXT(), float_literal->value_);
  GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(const_op);
  const_op->GetResult()->SetType(GET_TYPE_CONTEXT()->GetFloatType());
  // Map the result of the const operation with the float literal ExprNode*
  GET_MLIR_CONTEXT()->GetResourceContext()->MapExprToValue(float_literal, const_op->GetResult());
}

// Statement nodes
void MLIRVisitor::Visit(frontend::ast::BlockStmtNode *block_stmt) {
  // The block is different from the block in MLIR.
  // So we don't need to create a new block for the block statement,
  // we can just visit all the statements in the block statement directly
  for (const auto &stmt : block_stmt->block_items_) {
    stmt->Accept(this);
  }
}

void MLIRVisitor::Visit(frontend::ast::AssignStmtNode *assign_stmt) {
  auto &lval = assign_stmt->lval_;
  auto &expr = assign_stmt->expr_;
  // Since the lval has been guaranteed to be a LVal expr in parser
  auto lval_expr = static_cast<frontend::ast::LValExprNode *>(lval.get());
  auto lval_ptr = HandleLValExpr(lval_expr, true);
  expr->Accept(this);
  auto value = GET_MLIR_CONTEXT()->GetResourceContext()->GetValueFromExpr(expr.get());
  // Create a var.set operation to set the value of the variables
  auto var_set_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarSetOp>(GET_MLIR_CONTEXT(), lval_ptr, value);
  GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(var_set_op);
  // It should not generate any value, even though I set the result
  var_set_op->GetRes()->SetType(GET_TYPE_CONTEXT()->GetUnknownType());
  // Add use for the value and pointer
  lval_ptr->AddUse(var_set_op, 0);
  value->AddUse(var_set_op, 1);
}

void MLIRVisitor::Visit(frontend::ast::ExprStmtNode *expr_stmt) {
  // Just visit the expression, and ignore the result
  auto expr = expr_stmt->expr_.get();
  // if the expression is null, which in source code is just a semicolon,
  // we can just ignore it
  if (expr == nullptr) {
    return;
  }
  expr->Accept(this);
}

void MLIRVisitor::Visit([[maybe_unused]] frontend::ast::IfStmtNode *if_stmt) {}
void MLIRVisitor::Visit([[maybe_unused]] frontend::ast::WhileStmtNode *while_stmt) {}
void MLIRVisitor::Visit([[maybe_unused]] frontend::ast::BreakStmtNode *break_stmt) {}
void MLIRVisitor::Visit([[maybe_unused]] frontend::ast::ContinueStmtNode *continue_stmt) {}
void MLIRVisitor::Visit([[maybe_unused]] frontend::ast::ReturnStmtNode *return_stmt) {}
// Variable nodes includes decl and initializer
void MLIRVisitor::Visit(frontend::ast::VarDefNode *var_def) {
  // Since the variable definition has been checked and stored in semantic phase
  // The dimensions have been calculated
  var_def->init_values_->Accept(this);

  /**
   * @brief variable definition, we have stored the symbolId
   */
  auto symbol_id = var_def->symbol_id_;
  auto entry = GET_SYMBOLTABLE_CONTEXT()->GetVarEntryById(symbol_id);
  // Since the definition of a variable, if it has a dimension,
  // the dimensions have been calculated in the semantic phase, and stored in the VarEntry,
  // So we can just get the dimensions from the VarEntry, and use it to create the var.def
  auto dimensions = entry->dimensions_;

  /**
   * initialize the variable
   */
  auto results = flatten_helper_.FlattenInitializer(var_def->init_values_.get(), dimensions, initializer::HandleInIR);

  // Results are a list of exprnode*, we use the map to get the corresponding value in MLIR,
  // and then we will create the initializer operation to initialize the variable
  std::vector<mlir::Value *> init_values;
  for (const auto &result : results.GetInitializers()) {
    auto init = static_cast<initializer::MLIRInitializer *>(result.get());
    auto value = GET_MLIR_CONTEXT()->GetResourceContext()->GetValueFromExpr(init->GetValue());
    init_values.emplace_back(value);
  }

  /**
   * since we are defining a variable, we need to create a var.def
   * and record the varentry->value to result of the var.def,
   * so that we can deal with the usage of this variable later(espacially in LVal)
   */
  auto var_def_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarDefOp>(
      GET_MLIR_CONTEXT(), dimensions, init_values, entry->is_global_, entry->is_const_);
  GET_MLIR_CONTEXT()->GetResourceContext()->MapVarToValue(entry, var_def_op->GetRes());
}
void MLIRVisitor::Visit(frontend::ast::GlobalVarDeclNode *global_var_decl) {
  for (const auto &var_def : global_var_decl->defs_) {
    var_def->Accept(this);
  }
}
void MLIRVisitor::Visit(frontend::ast::LocalVarDeclNode *local_var_decl) {
  for (const auto &var_def : local_var_decl->defs_) {
    var_def->Accept(this);
  }
}
void MLIRVisitor::Visit(frontend::ast::InitListNode *init_list) {
  for (const auto &init_val : init_list->init_values_) {
    init_val->Accept(this);
  }
}
void MLIRVisitor::Visit(frontend::ast::InitValNode *init_val) { init_val->value_->Accept(this); }
// Function nodes
void MLIRVisitor::Visit(frontend::ast::FuncFParamNode *func_fparam) {
  auto value = GET_MLIR_CONTEXT()->CreateValue();
  value->SetType(func_fparam->type_);
  auto entry = GET_SYMBOLTABLE_CONTEXT()->GetVarEntryById(func_fparam->symbol_id_);
  // We regard the function parameter as a local variable,
  // so we can just map the entry to the value
  GET_MLIR_CONTEXT()->GetResourceContext()->MapVarToValue(entry, value);
}

void MLIRVisitor::Visit(frontend::ast::FuncDefNode *func_def) {
  /**
   * @brief A funcdef operation should be like:
   * @example:
   * func @func_name (arg1_type, arg2_type, ...) -> return_type
   * region {
   *   block {
   *     operations
   *   }
   * }
   */
  std::vector<mlir::Value *> args;
  for (const auto &param : func_def->parameters_) {
    param->Accept(this);
    auto entry = GET_SYMBOLTABLE_CONTEXT()->GetVarEntryById(param->symbol_id_);
    auto value = GET_MLIR_CONTEXT()->GetResourceContext()->GetValueFromVar(entry);
    args.emplace_back(value);
  }

  auto func_def_op = GET_MLIR_CONTEXT()->CreateOperation<middleend::mlir::func::FuncDefOp>(
      GET_MLIR_CONTEXT(), func_def->func_name_, args, func_def->return_type_);
  GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(func_def_op);
  GET_MLIR_CONTEXT()->SetCurrentRegion(func_def_op->regions_[0]);
  GET_MLIR_CONTEXT()->SetInsertPoint(func_def_op->regions_[0]->blocks_[0]);

  // Add use for the arguments
  for (size_t i = 0; i < args.size(); ++i) {
    args[i]->AddUse(func_def_op, i);
  }
  // Set current function definition
  GET_MLIR_CONTEXT()->SetCurrentFuncDefOp(func_def_op);
  // Visit the body of the function
  func_def->body_->Accept(this);
}

}  // namespace middleend