/**
 * @brief This file is the implementation of the MLIRVisitor class,
 * which will be used to traverse the AST and generate the MLIR IR.
 */
#include "middleend/builder/mlir/mlirvisitor.h"
#include <cstddef>
#include <string>
#include "frontend/ast/ast_decl.h"
#include "frontend/ast/ast_expr.h"
#include "frontend/ast/ast_spec.h"
#include "frontend/ast/ast_utils.h"
#include "middleend/mlir/dialect/arithm/arithm.h"
#include "middleend/mlir/dialect/func/func.h"
#include "middleend/mlir/dialect/func/return.h"
#include "middleend/mlir/dialect/scf/scf.h"
#include "middleend/mlir/dialect/var/var.h"
#include "middleend/mlir/utils/struct/block.h"
#include "middleend/mlir/utils/struct/region.h"
#include "middleend/mlir/utils/value/value.h"
#include "utils/flatten/mlirinit.h"
#include "utils/type/type.h"
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
    BINARY_OPERATOR_HANDLER(EQUAL, Equal)
    BINARY_OPERATOR_HANDLER(NEQUAL, NotEqual)
    BINARY_OPERATOR_HANDLER(LESS, Less)
    BINARY_OPERATOR_HANDLER(LEQUAL, LessEqual)
    BINARY_OPERATOR_HANDLER(GREATER, Greater)
    BINARY_OPERATOR_HANDLER(GEQUAL, GreaterEqual)
    // BINARY_OPERATOR_HANDLER(AND, And)
    // BINARY_OPERATOR_HANDLER(OR, Or)
    case frontend::ast::OpCode::AND:
    case frontend::ast::OpCode::OR: {
      compiler_context->ReportError("Logical operators should not be regarded as normal binary operators");
    } break;
    default:
      compiler_context->ReportError("Unsupported binary operator: " + OpCodeToString(binary_expr->operator_));
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
    case frontend::ast::OpCode::POSITIVE: {
      result = operand;
    } break;
    default:
      compiler_context->ReportError("Unsupported unary operator: " + OpCodeToString(unary_expr->operator_));
  }
  return result;
}

inline auto MLIRVisitor::HandleLValExpr(frontend::ast::LValExprNode *lval_expr, bool get_ptr,
                                        bool &is_array_or_global) -> mlir::Value * {
  /**
   * @brief The lval expression shall be a variable
   */
  auto symbol_id = lval_expr->symbol_id_;
  is_array_or_global = true;
  auto entry = GET_SYMBOLTABLE_CONTEXT()->GetVarEntryById(symbol_id);
  if (entry == nullptr) {
    compiler_context_->ReportError(lval_expr, "variable is not defined");
    return nullptr;
  }
  // Since the variable is defined, we can get its value from the symbol table entry
  auto ptr = GET_MLIR_CONTEXT()->GetResourceContext()->GetValueFromVar(entry);
  if (ptr == nullptr) {
    compiler_context_->ReportError(lval_expr, "variable is not defined in the current context");
    return nullptr;
  }
  mlir::var::VarGetPtrOp *var_get_op = nullptr;
  // if the variable is a array
  if (entry->dimensions_.empty() && !entry->is_global_) {
    // Not an array variable, we can directly get the value from the symbol table entry
    // var_get_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarGetPtrOp>(GET_MLIR_CONTEXT(), ptr);
    // GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(var_get_op);
    // GET_MLIR_CONTEXT()->GetResourceContext()->MapExprToValue(lval_expr, var_get_op->GetRes());
    // auto type = entry->type_;
    // /* Add use and set operation which generates the result */
    // ptr->AddUse(var_get_op, 0);

    // var_get_op->GetRes()->SetType(type);
    GET_MLIR_CONTEXT()->GetResourceContext()->MapExprToValue(lval_expr, ptr);
    is_array_or_global = false;
    return ptr;
  }
  // Is an array
  auto indices = std::vector<mlir::Value *>{};
  for (const auto &index : lval_expr->indices_) {
    index->Accept(this);
    auto index_value = GET_MLIR_CONTEXT()->GetResourceContext()->GetValueFromExpr(index.get());
    if (index_value->GetType()->GetKind() != type::TypeKind::INT) {
      auto cast_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::CastOp>(
          GET_MLIR_CONTEXT(), index_value, index_value->GetType(), GET_TYPE_CONTEXT()->GetIntegerType());
      index_value->AddUse(cast_op, 0);
      GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(cast_op);
      index_value = cast_op->GetRes();
    }
    indices.emplace_back(index_value);
  }

  var_get_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarGetPtrOp>(GET_MLIR_CONTEXT(), ptr, indices);
  GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(var_get_op);
  GET_MLIR_CONTEXT()->GetResourceContext()->MapExprToValue(lval_expr, var_get_op->GetRes());
  // The type of the lval expression has been calculated in the semantic analysis phase.
  auto type = lval_expr->GetType();
  if (type->GetKind() == type::TypeKind::PTR) {
    // The lval is a pointer, so we can just set the type
    var_get_op->GetRes()->SetType(type);
  } else {
    auto ptr_type = GET_TYPE_CONTEXT()->GetPtrType(type);
    var_get_op->GetRes()->SetType(ptr_type);
  }
  /* Add use and set operation which generates the result */
  ptr->AddUse(var_get_op, 0);
  int i = 1;
  for (const auto &index_value : indices) {
    index_value->AddUse(var_get_op, i++);
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

inline auto MLIRVisitor::MapLValExprToValue(frontend::ast::LValExprNode *lval_expr, mlir::Value *value) -> void {
  auto entry = GET_SYMBOLTABLE_CONTEXT()->GetVarEntryById(lval_expr->symbol_id_);
  if (entry == nullptr) {
    compiler_context_->ReportError(lval_expr, "variable is not defined");
    return;
  }
  GET_MLIR_CONTEXT()->GetResourceContext()->MapVarToValue(entry, value);
  GET_MLIR_CONTEXT()->GetResourceContext()->MapExprToValue(lval_expr, value);
}

inline auto MLIRVisitor::HandleShortCircuitOperator(frontend::ast::BinaryExprNode *binary_expr) -> void {
  // TODO(loki): implement the short-circuit logical operator handling
  if (binary_expr->operator_ == frontend::ast::OpCode::AND) {
    // Handle the short-circuit logical AND operator
    // Since the lhs will always be calculated
    auto lhs_value = GET_MLIR_CONTEXT()->GetResourceContext()->GetValueFromExpr(binary_expr->left_.get());
    // not bool , do cast
    if (lhs_value->type_->GetKind() != type::TypeKind::BOOL) {
      auto cast_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::CastOp>(
          GET_MLIR_CONTEXT(), lhs_value, lhs_value->type_, GET_TYPE_CONTEXT()->GetBooleanType());
      lhs_value->AddUse(cast_op, 0);
      GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(cast_op);
      lhs_value = cast_op->GetRes();
    }

    // Record current region and insert point
    auto current_region = GET_MLIR_CONTEXT()->GetStateContext()->GetCurrentRegion();
    auto current_insert_point = GET_MLIR_CONTEXT()->GetStateContext()->GetInsertPoint();

    // Else region return original value
    auto else_region = new mlir::Region();
    auto else_block = new mlir::Block();
    else_region->blocks_.emplace_back(else_block);
    auto else_yield_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::YieldOp>(GET_MLIR_CONTEXT(), lhs_value);
    else_block->AddOperation(else_yield_op);
    lhs_value->AddUse(else_yield_op, 0);

    // Create the scf.if operation for short-circuiting
    auto then_region = new mlir::Region();
    auto then_block = new mlir::Block();
    then_region->blocks_.emplace_back(then_block);
    // Remember to set and reset the current region and insert point while generating the then block
    GET_MLIR_CONTEXT()->SetCurrentRegion(then_region);
    GET_MLIR_CONTEXT()->SetInsertPoint(then_block);
    // Then in then block, generate the rhs value and the result of the binary expression
    auto rhs = binary_expr->right_.get();
    rhs->Accept(this);
    // get rhs result
    auto rhs_value = GET_MLIR_CONTEXT()->GetResourceContext()->GetValueFromExpr(rhs);

    if (rhs_value->type_->GetKind() != type::TypeKind::BOOL) {
      auto cast_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::CastOp>(
          GET_MLIR_CONTEXT(), rhs_value, rhs_value->type_, GET_TYPE_CONTEXT()->GetBooleanType());
      rhs_value->AddUse(cast_op, 0);
      GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(cast_op);
      rhs_value = cast_op->GetRes();
    }

    // Create the scf.if operation
    /**
     * @note:
     * - cond : the lhs value
     * - then yield: yield rhs value
     */
    // Add yield operation in then block
    auto yield_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::YieldOp>(GET_MLIR_CONTEXT(), rhs_value);
    then_block->AddOperation(yield_op);
    auto if_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::IfOp>(GET_MLIR_CONTEXT(), 1, lhs_value, then_region,
                                                                      else_region);

    // Reset region and insert point
    GET_MLIR_CONTEXT()->SetCurrentRegion(current_region);
    GET_MLIR_CONTEXT()->SetInsertPoint(current_insert_point);
    GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(if_op);
    auto result = if_op->GetResultByIndex(0);
    result->SetType(binary_expr->GetType());
    // Map the binary expression to the result of the if operation
    GET_MLIR_CONTEXT()->GetResourceContext()->MapExprToValue(binary_expr, result);
  } else if (binary_expr->operator_ == frontend::ast::OpCode::OR) {
    // Handle the short-circuit logical OR operator
    // Since the lhs will always be calculated
    auto lhs_value = GET_MLIR_CONTEXT()->GetResourceContext()->GetValueFromExpr(binary_expr->left_.get());
    // not bool , do cast
    if (lhs_value->type_->GetKind() != type::TypeKind::BOOL) {
      auto cast_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::CastOp>(
          GET_MLIR_CONTEXT(), lhs_value, lhs_value->type_, GET_TYPE_CONTEXT()->GetBooleanType());
      lhs_value->AddUse(cast_op, 0);
      GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(cast_op);
      lhs_value = cast_op->GetRes();
    }

    // Record current region and insert point
    auto current_region = GET_MLIR_CONTEXT()->GetStateContext()->GetCurrentRegion();
    auto current_insert_point = GET_MLIR_CONTEXT()->GetStateContext()->GetInsertPoint();

    // Create the scf.if operation for short-circuiting
    auto then_region = new mlir::Region();
    auto then_block = new mlir::Block();
    then_region->blocks_.emplace_back(then_block);
    // Remember to set and reset the current region and insert point while generating the then block
    auto then_yield_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::YieldOp>(GET_MLIR_CONTEXT(), lhs_value);
    then_block->AddOperation(then_yield_op);

    // in or(||) operater, the rhs will only be calculated when the lhs is false,
    // which means rhs should be generated in the else block
    auto else_region = new mlir::Region();
    auto else_block = new mlir::Block();
    else_region->blocks_.emplace_back(else_block);
    GET_MLIR_CONTEXT()->SetCurrentRegion(else_region);
    GET_MLIR_CONTEXT()->SetInsertPoint(else_block);

    // Generate the rhs value in the else block
    auto rhs = binary_expr->right_.get();
    rhs->Accept(this);

    auto rhs_value = GET_MLIR_CONTEXT()->GetResourceContext()->GetValueFromExpr(rhs);
    if (rhs_value->type_->GetKind() != type::TypeKind::BOOL) {
      auto cast_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::CastOp>(
          GET_MLIR_CONTEXT(), rhs_value, rhs_value->type_, GET_TYPE_CONTEXT()->GetBooleanType());
      rhs_value->AddUse(cast_op, 0);
      GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(cast_op);
      rhs_value = cast_op->GetRes();
    }
    auto else_yield_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::YieldOp>(GET_MLIR_CONTEXT(), rhs_value);
    lhs_value->AddUse(else_yield_op, 0);
    else_block->AddOperation(else_yield_op);
    auto if_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::IfOp>(GET_MLIR_CONTEXT(), 1, lhs_value, then_region,
                                                                      else_region);

    // Reset region and insert point
    GET_MLIR_CONTEXT()->SetCurrentRegion(current_region);
    GET_MLIR_CONTEXT()->SetInsertPoint(current_insert_point);
    GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(if_op);
    auto result = if_op->GetResultByIndex(0);
    result->SetType(binary_expr->GetType());
    // Map the binary expression to the result of the if operation
    GET_MLIR_CONTEXT()->GetResourceContext()->MapExprToValue(binary_expr, result);
  }
}

inline auto MLIRVisitor::AddDefaultReturn(mlir::func::FuncDefOp *func_def_op) -> void {
  // Since we will do optimization to delete redundant return operations in the future,
  // so we can just add a return operation to the end of the function definition with a default value
  auto body_region = func_def_op->regions_[0];
  auto body_block = body_region->blocks_[0];
  auto body_end_op = body_block->operations_.empty() ? nullptr : body_block->operations_.back();
  auto return_type = func_def_op->GetReturnType();
  if (body_end_op == nullptr || mlir::DynamicOrNull<mlir::func::ReturnOp>(body_end_op) == nullptr) {
    // Since we only allow return: int, float, void
    if (return_type->GetKind() == type::TypeKind::VOID) {
      auto return_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::func::ReturnOp>(GET_MLIR_CONTEXT(), return_type);
      body_block->AddOperation(return_op);
    } else if (return_type->GetKind() == type::TypeKind::INT) {
      mlir::Value *const_value;
      auto existing_const_op = GET_MLIR_CONTEXT()->GetResourceContext()->GetConstOpFromConstValue(std::to_string(0));
      if (existing_const_op == nullptr) {
        auto const_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::ConstOp>(GET_MLIR_CONTEXT(), 0);
        body_block->AddOperation(const_op);
        const_op->GetRes()->SetType(GET_TYPE_CONTEXT()->GetIntegerType());
        GET_MLIR_CONTEXT()->GetResourceContext()->SetConstOpForConstValue(std::to_string(0), const_op);
        const_value = const_op->GetRes();
      } else {
        const_value = existing_const_op->GetRes();
      }

      // Return 0 for int return type
      auto return_op =
          GET_MLIR_CONTEXT()->CreateOperation<mlir::func::ReturnOp>(GET_MLIR_CONTEXT(), return_type, const_value);
      // Add use for the const value since it is used in the return operation
      const_value->AddUse(return_op, 0);
      body_block->AddOperation(return_op);
    } else if (return_type->GetKind() == type::TypeKind::FLOAT) {
      mlir::Value *const_value;
      auto existing_const_op = GET_MLIR_CONTEXT()->GetResourceContext()->GetConstOpFromConstValue(std::to_string(0.0F));
      if (existing_const_op == nullptr) {
        auto const_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::ConstOp>(GET_MLIR_CONTEXT(), 0.0F);
        body_block->AddOperation(const_op);
        const_op->GetRes()->SetType(GET_TYPE_CONTEXT()->GetFloatType());
        GET_MLIR_CONTEXT()->GetResourceContext()->SetConstOpForConstValue(std::to_string(0.0F), const_op);
        const_value = const_op->GetRes();
      } else {
        const_value = existing_const_op->GetRes();
      }

      auto return_op =
          GET_MLIR_CONTEXT()->CreateOperation<mlir::func::ReturnOp>(GET_MLIR_CONTEXT(), return_type, const_value);
      // Add use for the const value since it is used in the return operation
      const_value->AddUse(return_op, 0);
      body_block->AddOperation(return_op);
    }
  }
}

// RootNode
void MLIRVisitor::Visit(frontend::ast::RootNode *root) {
  // Create the module operation, which is the top-level operation in MLIR
  auto module_op = GET_MLIR_CONTEXT()->CreateOperation<middleend::mlir::ModuleOp>();
  GET_MLIR_CONTEXT()->SetModuleOp(module_op);
  GET_MLIR_CONTEXT()->SetInsertPoint(module_op->regions_[0]->blocks_[0]);
  GET_MLIR_CONTEXT()->SetCurrentRegion(module_op->regions_[0]);
  GET_MLIR_CONTEXT()->RegisterLibraryFunction();
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
// After finishing ifOp, since they can be regard as a special ifop
void MLIRVisitor::Visit(frontend::ast::BinaryExprNode *binary_expr) {
  auto lhs = binary_expr->left_.get();
  lhs->Accept(this);
  // Since the lhs will always be visited, no matter short-circuit or not

  /**
   * @note: for and(&&) operator, it will be translated to:
   * scf.if %lhs_value {
   *  // then block
   *  scf.yield rhs_value
   * }
   * @note for or(||) operator, it will be translated to:
   * scf.if %lhs_value {
   *   // then block
   *   // empty
   * } else {
   *   // else block
   *   scf.yield rhs_value
   * }
   */
  if (IsShortCircuitLogicalOperator(binary_expr)) {
    HandleShortCircuitOperator(binary_expr);
    return;
  }

  auto rhs = binary_expr->right_.get();
  rhs->Accept(this);

  /* Get the type of the binary expression */
  auto type = binary_expr->GetType();

  auto lhs_value = GET_MLIR_CONTEXT()->GetResourceContext()->GetValueFromExpr(lhs);
  auto rhs_value = GET_MLIR_CONTEXT()->GetResourceContext()->GetValueFromExpr(rhs);
  // Do cast to ensure the type of the left and right operands,
  // are the same with the type we decided in the semantic analysis phase.

  // If not logical operator,
  // we need to ensure the type of the operands
  // are the same with the type of the binary expression.
  if (!IsLogicalOperator(binary_expr)) {
    if (lhs->GetType() != type) {
      auto cast_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::CastOp>(GET_MLIR_CONTEXT(), lhs_value,
                                                                               lhs->GetType(), type);
      GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(cast_op);
      lhs_value->AddUse(cast_op, 0);
      lhs_value = cast_op->GetRes();
    }
    if (rhs->GetType() != type) {
      auto cast_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::CastOp>(GET_MLIR_CONTEXT(), rhs_value,
                                                                               rhs->GetType(), type);
      GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(cast_op);
      rhs_value->AddUse(cast_op, 0);
      rhs_value = cast_op->GetRes();
    }
  } else {
    // Is logical operator,
    // we only need to ensure the type of the operands are the same,
    // since the result of the logical operator must be i1 type,
    if (lhs->GetType() != rhs->GetType()) {
      // if left_prior is true, we will cast the right operand to the left_type
      auto left_prior =
          lhs->GetType()->GetKind() == type::TypeKind::FLOAT ||
          (lhs->GetType()->GetKind() == type::TypeKind::INT && rhs->GetType()->GetKind() == type::TypeKind::BOOL);
      auto cast_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::CastOp>(
          GET_MLIR_CONTEXT(), left_prior ? rhs_value : lhs_value, left_prior ? rhs->GetType() : lhs->GetType(),
          left_prior ? lhs->GetType() : rhs->GetType());
      GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(cast_op);
      if (left_prior) {
        rhs_value->AddUse(cast_op, 0);
        rhs_value = cast_op->GetRes();
      } else {
        lhs_value->AddUse(cast_op, 0);
        lhs_value = cast_op->GetRes();
      }
    }
  }

  /**
   * @brief we use the map to get value from the ExprNode*,
   * since every expression node will ONLY generate one value,
   * so we can just use the expression node as the key to get the value from the map, and then we can use the value to
   * build the operation for this binary expression
   */
  if (lhs_value->type_ != rhs_value->type_) {
    compiler_context_->ReportError(
        binary_expr, "BinaryOp: LHS and RHS should be the same type, lhs type: " + lhs_value->type_->ToString() +
                         ", rhs type: " + rhs_value->type_->ToString());
    return;
  }
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
  auto func_def_op = GET_MLIR_CONTEXT()->GetResourceContext()->GetFuncDefOpFromFuncName(func_name);
  if (func_def_op == nullptr) {
    compiler_context_->ReportError(func_call, "function is not defined");
    return;
  }
  auto defined_types = func_def_op->GetArguments();
  for (size_t i = 0; i < func_call->params_.size(); ++i) {
    auto param = func_call->params_[i].get();
    auto type = defined_types[i]->GetType();
    if (param->GetType()->GetKind() == type::TypeKind::PTR) {
      // Since if the param is ptr and is not the same as the defined type,
      // it must be a mistake, so we do not need to worry about the cast.
      bool is_array;
      auto param_value = HandleLValExpr(static_cast<frontend::ast::LValExprNode *>(param), true, is_array);
      params.emplace_back(param_value);
    } else {
      param->Accept(this);
      auto param_value = GET_MLIR_CONTEXT()->GetResourceContext()->GetValueFromExpr(param);
      if (param->GetType() != type) {
        // Need to do cast to ensure the type of the parameter
        param_value = GET_MLIR_CONTEXT()->GetResourceContext()->GetValueFromExpr(param);
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
  func_call_op->GetRes()->SetType(return_type);
  GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(func_call_op);
  GET_MLIR_CONTEXT()->GetResourceContext()->MapExprToValue(func_call, func_call_op->GetRes());
  // Add use for the arguments
  for (size_t i = 0; i < params.size(); ++i) {
    params[i]->AddUse(func_call_op, i);
  }
}

void MLIRVisitor::Visit(frontend::ast::LValExprNode *lval_expr) {
  bool is_array;
  auto res = HandleLValExpr(lval_expr, false, is_array);
  if (res == nullptr) {
    compiler_context_->ReportError(lval_expr, "Failed to handle LVal expression.");
    return;
  }
}

void MLIRVisitor::Visit(frontend::ast::IntLiteralExprNode *int_literal) {
  auto existing_const_op =
      GET_MLIR_CONTEXT()->GetResourceContext()->GetConstOpFromConstValue(std::to_string(int_literal->value_));
  if (existing_const_op != nullptr) {
    GET_MLIR_CONTEXT()->GetResourceContext()->MapExprToValue(int_literal, existing_const_op->GetRes());
    return;
  }
  auto const_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::ConstOp>(GET_MLIR_CONTEXT(), int_literal->value_);
  const_op->GetRes()->SetType(GET_TYPE_CONTEXT()->GetIntegerType());
  // Insert the constant operation to the function's entry block,
  // so that the constant operation can be reused in the whole function,
  // since the constant value is immutable.

  auto func_op = GET_MLIR_CONTEXT()->GetCurrentFuncDefOp();
  if (func_op == nullptr) {
    GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(const_op);
  } else {
    auto entry_block = func_op->GetEntryBlock();
    entry_block->AddOperation(const_op);
  }

  // Map constant value to the constant operation
  GET_MLIR_CONTEXT()->GetResourceContext()->SetConstOpForConstValue(std::to_string(int_literal->value_), const_op);
  // Map the result of the const operation with the int literal ExprNode*
  GET_MLIR_CONTEXT()->GetResourceContext()->MapExprToValue(int_literal, const_op->GetRes());
}

void MLIRVisitor::Visit(frontend::ast::FloatLiteralExprNode *float_literal) {
  auto existing_const_op =
      GET_MLIR_CONTEXT()->GetResourceContext()->GetConstOpFromConstValue(std::to_string(float_literal->value_));
  if (existing_const_op != nullptr) {
    GET_MLIR_CONTEXT()->GetResourceContext()->MapExprToValue(float_literal, existing_const_op->GetRes());
    return;
  }
  auto const_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::ConstOp>(GET_MLIR_CONTEXT(), float_literal->value_);
  const_op->GetRes()->SetType(GET_TYPE_CONTEXT()->GetFloatType());

  // Insert the constant operation to the function's entry block,
  // so that the constant operation can be reused in the whole function,
  // since the constant value is immutable.

  auto func_op = GET_MLIR_CONTEXT()->GetCurrentFuncDefOp();
  if (func_op == nullptr) {
    GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(const_op);
  } else {
    auto entry_block = func_op->GetEntryBlock();
    entry_block->AddOperation(const_op);
  }

  // Map constant value to the constant operation
  GET_MLIR_CONTEXT()->GetResourceContext()->SetConstOpForConstValue(std::to_string(float_literal->value_), const_op);
  // Map the result of the const operation with the float literal ExprNode*
  GET_MLIR_CONTEXT()->GetResourceContext()->MapExprToValue(float_literal, const_op->GetRes());
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
  bool is_array;
  auto lval_ptr = HandleLValExpr(lval_expr, true, is_array);
  expr->Accept(this);
  auto value = GET_MLIR_CONTEXT()->GetResourceContext()->GetValueFromExpr(expr.get());
  if (value == nullptr) {
    compiler_context_->ReportError(expr.get(), "Failed to get value from expression.");
    return;
  }

  if (is_array) {  // Create a var.set operation to set the value of the variables
    auto var_set_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarSetOp>(GET_MLIR_CONTEXT(), lval_ptr, value);
    GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(var_set_op);
    // It should not generate any value, even though I set the result
    var_set_op->GetRes()->SetType(GET_TYPE_CONTEXT()->GetUnknownType());
    // Add use for the value and pointer
    lval_ptr->AddUse(var_set_op, 0);
    value->AddUse(var_set_op, 1);
  } else {
    if (lval_ptr->type_ != value->type_) {
      auto cast_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::CastOp>(GET_MLIR_CONTEXT(), value, value->type_,
                                                                               lval_ptr->type_);
      GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(cast_op);
      value->AddUse(cast_op, 0);
      value = cast_op->GetRes();
    }
    // If the lval is not an array, we can just map the lval to the new value
    // We don't want change the original value of the expr, so we add a operation which is copy the value
    auto copy_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::CopyOp>(GET_MLIR_CONTEXT(), value);
    copy_op->GetRes()->SetType(value->GetType());
    GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(copy_op);
    MapLValExprToValue(lval_expr, copy_op->GetRes());
  }
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

void MLIRVisitor::Visit(frontend::ast::IfStmtNode *if_stmt) {
  // Start with cond, it is always execute
  auto cond = if_stmt->condition_.get();
  cond->Accept(this);
  // Generate the bool value for the condition
  auto cond_value = GET_MLIR_CONTEXT()->GetResourceContext()->GetValueFromExpr(cond);
  if (cond_value == nullptr) {
    compiler_context_->ReportError(cond, "Failed to get value from condition expression.");
    return;
  }

  if (cond_value->type_->GetKind() != type::TypeKind::BOOL) {
    auto cast_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::CastOp>(
        GET_MLIR_CONTEXT(), cond_value, cond_value->type_, GET_TYPE_CONTEXT()->GetBooleanType());
    cond_value->AddUse(cast_op, 0);
    GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(cast_op);
    cond_value = cast_op->GetRes();
  }

  // Since the if statement will change some of the values in the variable map,
  // We will save the current variable map before visiting the if statement,
  // and restore it after visiting the if statement(then), and do the same if there is else statement
  auto var_map = GET_MLIR_CONTEXT()->GetResourceContext()->GetVarMap();
  // Restore the top region and block
  auto current_region = GET_MLIR_CONTEXT()->GetCurrentRegion();
  auto current_block = GET_MLIR_CONTEXT()->GetInsertPoint();

  // Record all variables which are changed in the branch
  std::vector<symboltable::VarEntry *> changed_vars;
  // Record IfOp
  mlir::scf::IfOp *if_op = nullptr;

  // Then need to deal with the then statement
  auto then_region = new mlir::Region();
  GET_MLIR_CONTEXT()->SetCurrentRegion(then_region);
  auto then_block = new mlir::Block();
  then_region->blocks_.emplace_back(then_block);
  GET_MLIR_CONTEXT()->SetInsertPoint(then_block);
  auto then_branch = if_stmt->then_branch_.get();
  then_branch->Accept(this);
  // Finish visiting the then statement, need to get the then variable
  auto then_var_map = GET_MLIR_CONTEXT()->GetResourceContext()->GetVarMap();

  auto else_branch = if_stmt->else_branch_ == nullptr ? nullptr : if_stmt->else_branch_.get();

  if (else_branch == nullptr) {
    // No else statement, we can just create an if operation and then get yield op
    // First deal with the yield
    std::vector<mlir::Value *> then_yield_values;
    std::vector<mlir::Value *> else_yield_values;

    // Since the then statement may def some variables which will not be accessed
    // in other statements, so we only need to yield the variables
    // which are changed in the then statement,
    // and the value of these variables in the else block
    // should be the same as the original value
    // before visiting the then statement
    for (const auto [entry, value] : var_map) {
      auto then_value = then_var_map[entry];
      // Collect all variables which are changed in the branch
      if (then_value != value) {
        changed_vars.emplace_back(entry);
        then_yield_values.emplace_back(then_value);
        else_yield_values.emplace_back(value);
      }
    }
    // Create the yield operation in the then block
    /**
     * @note: yield op has no result
     */
    auto then_yield_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::YieldOp>(GET_MLIR_CONTEXT(), then_yield_values);
    GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(then_yield_op);

    // No else statement, we should restore the original value in the value map before then
    auto else_region = new mlir::Region();
    auto else_block = new mlir::Block();
    else_region->blocks_.emplace_back(else_block);
    auto else_yield_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::YieldOp>(GET_MLIR_CONTEXT(), else_yield_values);
    else_block->AddOperation(else_yield_op);

    for (size_t i = 0; i < then_yield_values.size(); ++i) {
      then_yield_values[i]->AddUse(then_yield_op, i);
      else_yield_values[i]->AddUse(else_yield_op, i);
    }
    // Restore the variable map before visiting the else statement
    GET_MLIR_CONTEXT()->GetResourceContext()->SetVarMap(var_map);

    // Then create the if operation
    if_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::IfOp>(GET_MLIR_CONTEXT(), changed_vars.size(), cond_value,
                                                                 then_region, else_region);
  } else {
    GET_MLIR_CONTEXT()->GetResourceContext()->SetVarMap(var_map);
    // Has else statement, we need to create the else region
    // and then get the yield op in both then and else block
    auto else_region = new mlir::Region();
    GET_MLIR_CONTEXT()->SetCurrentRegion(else_region);
    auto else_block = new mlir::Block();
    else_region->blocks_.emplace_back(else_block);
    GET_MLIR_CONTEXT()->SetInsertPoint(else_block);
    else_branch->Accept(this);
    // Finish visiting the else statement
    auto else_var_map = GET_MLIR_CONTEXT()->GetResourceContext()->GetVarMap();

    std::vector<mlir::Value *> then_yield_values;
    std::vector<mlir::Value *> else_yield_values;
    // Collect all variables which are changed in the branch
    for (const auto [entry, value] : var_map) {
      auto then_value = then_var_map[entry];
      auto else_value = else_var_map[entry];
      if (then_value != else_value) {
        changed_vars.emplace_back(entry);
        then_yield_values.emplace_back(then_value);
        else_yield_values.emplace_back(else_value);
      }
      // Since over here, the same value in both then and else
      // means the value is not changed or set to a constant
      // We can just set the value in the variable map to the new value
      /**
       * @note: This will never be the same value since it is SSA form
       */
      // if (then_value == else_value && then_value != value) {
      //   var_map[entry] = then_value;
      // }
    }

    // Create the yield operation in the then block
    auto then_yield_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::YieldOp>(GET_MLIR_CONTEXT(), then_yield_values);
    GET_MLIR_CONTEXT()->SetCurrentRegion(then_region);
    GET_MLIR_CONTEXT()->SetInsertPoint(then_block);
    GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(then_yield_op);
    for (size_t i = 0; i < then_yield_values.size(); ++i) {
      then_yield_values[i]->AddUse(then_yield_op, i);
    }

    // Create the yield operation in the else block
    auto else_yield_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::YieldOp>(GET_MLIR_CONTEXT(), else_yield_values);
    GET_MLIR_CONTEXT()->SetCurrentRegion(else_region);
    GET_MLIR_CONTEXT()->SetInsertPoint(else_block);
    GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(else_yield_op);
    for (size_t i = 0; i < else_yield_values.size(); ++i) {
      else_yield_values[i]->AddUse(else_yield_op, i);
    }
    GET_MLIR_CONTEXT()->GetResourceContext()->SetVarMap(var_map);

    // Then create the if operation
    if_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::IfOp>(GET_MLIR_CONTEXT(), changed_vars.size(), cond_value,
                                                                 then_region, else_region);
  }

  GET_MLIR_CONTEXT()->SetCurrentRegion(current_region);
  GET_MLIR_CONTEXT()->SetInsertPoint(current_block);
  GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(if_op);

  /**
   * @brief Set all changed variables in the YieldOp to the value of the results of if operation
   */
  for (size_t i = 0; i < changed_vars.size(); ++i) {
    auto entry = changed_vars[i];
    auto original_value = var_map[entry];
    auto type = original_value->GetType();
    auto value = if_op->GetResultByIndex(i);
    value->SetType(type);
    GET_MLIR_CONTEXT()->GetResourceContext()->MapVarToValue(entry, value);
  }
}

void MLIRVisitor::Visit(frontend::ast::WhileStmtNode *while_stmt) {
  /**
   * @brief In AST, the while statement is like follows:
   * while( cond ) {
   *  body
   * }
   * @brief In MLIR, we will translate it to:
   * scf.while {
   *  cond_block:
   *    // generate code for cond
   *    scf.condition %cond_value, %iter_args
   *  body_block(%iter_args) :
   *    // generate code for body
   *    scf.yield %iter_args
   * }
   * @note: we need to get the iter_args throughout the loop
   * My current design idea is:

   * 1. Before visiting the while statement, save current variable map,
   * which based on the idea that we only need to consider all variables defined before the while statement
   * To be more strict, de facto we need to erase local variables defined other local place,
   * since currently we just simply dealloca the variable entry in the destroy phase.
   * However, it has no side effect on the correctness of the generation, since we have gone through
   * the semantic analysis phase, and we have guaranteed that there will be no access to the local variable in other
   * scope, so it will just add some redundant scan in the variable map, but it will not cause any error in the
   * generation.

   * 2. Then we need to create the ``cond`` region, since we will generate the condition part over there.
   * Meanwhile, we need to collect all variables which are used in the condition part,

   * 3. Then we need to create the ``body`` region, and generate the body part in this region,
   * Meanwhile we also need to collect all variables which are used in the body part,
   * and then we can get the iter_args by merging the variables we collected in both stages.

   * 4. (optional) Since there might be `break` or `continue`, we need to maintain a set,
   * of which collects all the variables which are changed in the body
   * as well as the current state of the variable map when `break` or
   * `continue` is called, so that we can exit the loop and yield the corresponding variables
   * with the correct value in the `break` or `continue` case.

   * However,
   * @note Since we don't want to import control flow in this stage,
   * we need to decide where to exit in `break` and `continue` however,
   * so perhaps we need to record the whileOp to the breakOp or continueOp,
   * so that during the lowering phase, we can just lower the breakOp and continueOp to the
   * corresponding branch(WhileOp stored generated) to exit the loop.
   * Of course, it will cause more complicated `phi` instruction.
   */

  // Record the current variable map before visiting the while statement
  auto var_map = GET_MLIR_CONTEXT()->GetResourceContext()->GetVarMap();
  // Restore the top region and block
  auto current_region = GET_MLIR_CONTEXT()->GetCurrentRegion();
  auto current_insert_point = GET_MLIR_CONTEXT()->GetInsertPoint();

  // Enter a loop
  GET_MLIR_CONTEXT()->GetResourceContext()->EnterLoop();

  // cond region
  auto cond_region = new mlir::Region();
  auto cond_block = new mlir::Block();
  cond_region->blocks_.emplace_back(cond_block);
  GET_MLIR_CONTEXT()->SetCurrentRegion(cond_region);
  GET_MLIR_CONTEXT()->SetInsertPoint(cond_block);
  auto cond = while_stmt->condition_.get();
  cond->Accept(this);

  // since the condition will not change the value of the variables
  auto cond_value = GET_MLIR_CONTEXT()->GetResourceContext()->GetValueFromExpr(cond);
  if (cond_value == nullptr) {
    compiler_context_->ReportError(cond, "Failed to get value from condition expression.");
    return;
  }

  if (cond_value->GetType()->GetKind() != type::TypeKind::BOOL) {
    // Do cast to ensure the type of the condition value is boolean
    auto cast_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::CastOp>(
        GET_MLIR_CONTEXT(), cond_value, cond_value->GetType(), GET_TYPE_CONTEXT()->GetBooleanType());
    GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(cast_op);
    cond_value->AddUse(cast_op, 0);
    cond_value = cast_op->GetRes();
  }

  // Then generate the condition operation
  auto condition_op =
      GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::ConditionOp>(GET_MLIR_CONTEXT(), cond_value, var_map);
  GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(condition_op);

  // body region
  auto body_region = new mlir::Region();
  auto body_block = new mlir::Block();
  body_region->blocks_.emplace_back(body_block);
  GET_MLIR_CONTEXT()->SetCurrentRegion(body_region);
  GET_MLIR_CONTEXT()->SetInsertPoint(body_block);
  auto body = while_stmt->body_.get();
  body->Accept(this);

  // Finish visiting the body statement, we can get the variable map to get loop carried variables
  auto new_var_map = GET_MLIR_CONTEXT()->GetResourceContext()->GetVarMap();
  std::vector<symboltable::VarEntry *> iter_args;
  for (const auto &[entry, value] : var_map) {
    auto new_value = new_var_map[entry];
    if (value != new_value) {
      iter_args.emplace_back(entry);
    }
  }

  // Create the yield operation in the body block
  std::vector<mlir::Value *> yield_values;
  for (const auto &entry : iter_args) {
    auto value = new_var_map[entry];
    yield_values.emplace_back(value);
  }
  auto yield_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::YieldOp>(GET_MLIR_CONTEXT(), yield_values);
  GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(yield_op);
  for (size_t i = 0; i < yield_values.size(); ++i) {
    yield_values[i]->AddUse(yield_op, i);
  }

  // Then create the while operation
  auto while_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::WhileOp>(GET_MLIR_CONTEXT(), iter_args.size(),
                                                                          cond_region, body_region);

  GET_MLIR_CONTEXT()->SetCurrentRegion(current_region);
  GET_MLIR_CONTEXT()->SetInsertPoint(current_insert_point);
  // Set the value of the iter_args to the value of the results of while operation
  for (size_t i = 0; i < iter_args.size(); ++i) {
    auto entry = iter_args[i];
    auto value = while_op->GetResultByIndex(i);
    value->SetType(var_map[entry]->GetType());
  }

  // We need to feed back the iter_args to the condition/break/continue
  condition_op->SetLoopCarriedVars(iter_args);

  auto break_info = GET_MLIR_CONTEXT()->GetResourceContext()->GetBreakOp();
  for (auto break_op : break_info) {
    break_op->SetLoopCarriedVars(iter_args);
  }

  auto continue_info = GET_MLIR_CONTEXT()->GetResourceContext()->GetContinueOp();
  for (auto continue_op : continue_info) {
    continue_op->SetLoopCarriedVars(iter_args);
  }

  // In front of the while operation, we import an operation to initialize the iter_args,
  // which is used to initialize the loop carried variables,
  // and since the iter_args will be changed in the loop, we can just replace all the uses of the original value
  // with the value of the iter_args,
  // so that we can ensure the correctness of the loop carried variables in the loop
  auto original_values = std::vector<mlir::Value *>();
  for (const auto &entry : iter_args) {
    auto original_value = var_map[entry];
    original_values.emplace_back(original_value);
  }

  if (!original_values.empty()) {
    // auto inits_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::InitsOp>(GET_MLIR_CONTEXT(), original_values);
    // Create block args
    auto block_args = std::vector<mlir::Value *>();

    // GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(inits_op);
    for (size_t i = 0; i < original_values.size(); ++i) {
      auto arg = GET_MLIR_CONTEXT()->CreateValue();
      arg->SetType(original_values[i]->GetType());
      block_args.emplace_back(arg);
      original_values[i]->ReplaceAllUsesWith(arg);
      GET_MLIR_CONTEXT()->GetResourceContext()->SetArgValueName(arg, i);
    }

    // Add arguments to block and body block
    // These arguments will be used to lower to the phi operation in the lowering phase.
    for (auto arg : block_args) {
      cond_block->AddArgument(arg);
      body_block->AddArgument(arg);
    }
  }

  // Map the whileop's results to the iter_args,
  // so that after whileop, the following code can just get the value from the results of the whileop
  for (size_t i = 0; i < iter_args.size(); ++i) {
    auto entry = iter_args[i];
    auto value = while_op->GetResultByIndex(i);
    GET_MLIR_CONTEXT()->GetResourceContext()->MapVarToValue(entry, value);
  }

  GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(while_op);

  // Record whileop
  GET_MLIR_CONTEXT()->GetResourceContext()->AddLoop(while_op);

  // Exit the loop
  GET_MLIR_CONTEXT()->GetResourceContext()->ExitLoop();
}

void MLIRVisitor::Visit([[maybe_unused]] frontend::ast::BreakStmtNode *break_stmt) {
  // Create a break op and then wait for the whileop to fill the loop carried variables
  auto break_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::BreakOp>(GET_MLIR_CONTEXT());
  GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(break_op);
  // Record the break op to the resource context, so that we can feed back the loop carried variables after visiting the
  // while statement
  GET_MLIR_CONTEXT()->GetResourceContext()->AddBreakOp(break_op);
}

void MLIRVisitor::Visit([[maybe_unused]] frontend::ast::ContinueStmtNode *continue_stmt) {
  // Create a continue op and then wait for the whileop to fill the loop carried variables
  auto continue_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::ContinueOp>(GET_MLIR_CONTEXT());
  GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(continue_op);
  // Record the continue op to the resource context, so that we can feed back the loop carried variables after visiting
  // the while statement
  GET_MLIR_CONTEXT()->GetResourceContext()->AddContinueOp(continue_op);
}

void MLIRVisitor::Visit(frontend::ast::ReturnStmtNode *return_stmt) {
  auto expr = return_stmt->expr_ == nullptr ? nullptr : return_stmt->expr_.get();
  // Get defined return type
  auto type = GET_MLIR_CONTEXT()->GetCurrentFuncDefOp()->GetReturnType();

  if (expr != nullptr) {
    expr->Accept(this);
    auto value = GET_MLIR_CONTEXT()->GetResourceContext()->GetValueFromExpr(expr);
    if (value == nullptr) {
      compiler_context_->ReportError(expr, "Failed to get value from return expression.");
      return;
    }
    if (type != expr->GetType()) {
      // Need to do cast to ensure the type of the return value
      value = GET_MLIR_CONTEXT()->GetResourceContext()->GetValueFromExpr(expr);
      auto cast_op =
          GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::CastOp>(GET_MLIR_CONTEXT(), value, expr->GetType(), type);
      value->AddUse(cast_op, 0);
      GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(cast_op);
      value = cast_op->GetRes();
    }
    auto return_op = GET_MLIR_CONTEXT()->CreateOperation<middleend::mlir::func::ReturnOp>(GET_MLIR_CONTEXT(),
                                                                                          value->GetType(), value);

    GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(return_op);
    value->AddUse(return_op, 0);
  } else {
    auto return_op = GET_MLIR_CONTEXT()->CreateOperation<middleend::mlir::func::ReturnOp>(
        GET_MLIR_CONTEXT(), GET_TYPE_CONTEXT()->GetVoidType());
    GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(return_op);
  }
}

// Variable nodes includes decl and initializer
void MLIRVisitor::Visit(frontend::ast::VarDefNode *var_def) {
  // Since the variable definition has been checked and stored in semantic phase
  // The dimensions have been calculated
  if (var_def->init_values_) {
    var_def->init_values_->Accept(this);
  }

  /**
   * @brief variable definition, we have stored the symbolId
   */
  auto symbol_id = var_def->symbol_id_;
  auto entry = GET_SYMBOLTABLE_CONTEXT()->GetVarEntryById(symbol_id);

  // Results are a list of exprnode*, we use the map to get the corresponding value in MLIR,
  // and then we will create the initializer operation to initialize the variable

  /**
   * @note: if the variable is a global variable, we will create a def
   */
  if (!entry->dimensions_.empty() || entry->is_global_) {  // Since the definition of a variable, if it has a dimension,
    // the dimensions have been calculated in the semantic phase, and stored in the VarEntry,
    // So we can just get the dimensions from the VarEntry, and use it to create the var.def
    auto dimensions = entry->dimensions_;
    std::vector<mlir::Value *> init_values;

    /**
     * initialize the variable
     */
    if (var_def->init_values_ != nullptr) {
      auto results =
          flatten_helper_.FlattenInitializer(var_def->init_values_.get(), dimensions, initializer::HandleInIR);
      for (const auto &result : results.GetInitializers()) {
        auto init = static_cast<initializer::MLIRInitializer *>(result.get());
        // Since when the init is default initializer,
        // the value in MLIRInitializer is nullptr,
        // we can just set the init value to nullptr in this case
        if (init == nullptr) {
          init_values.emplace_back(nullptr);
          continue;
        }
        auto value = GET_MLIR_CONTEXT()->GetResourceContext()->GetValueFromExpr(init->GetValue());
        if (value == nullptr) {
          compiler_context_->ReportError(init->GetValue(), "Failed to get value from initializer expression.");
          return;
        }
        auto type = entry->type_;
        // Get the element type of the array
        // Since we need to make sure that the type of the initiaizer
        // same as the element type of the array.
        if (type->GetKind() == type::TypeKind::ARRAY) {
          type = static_cast<type::ArrayType *>(type)->GetElementType();
        }
        if (value->GetType() != type) {
          auto cast_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::CastOp>(GET_MLIR_CONTEXT(), value,
                                                                                   value->GetType(), type);
          value->AddUse(cast_op, 0);
          GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(cast_op);
          value = cast_op->GetRes();
        }
        init_values.emplace_back(value);
      }
    }
    // Provide empty initializer for the uninitialized variable,
    // since in C language, the uninitialized variable will be initialized to `0` by default
    // we will accomplish this by `memset` in the lowering phase.

    /**
     * since we are defining a variable, we need to create a var.def
     * and record the varentry->value to result of the var.def,
     * so that we can deal with the usage of this variable later(espacially in LVal)
     */
    // We only call def in array variable definition
    auto var_def_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarDefOp>(
        GET_MLIR_CONTEXT(), dimensions, init_values, entry->is_global_, entry->is_const_);
    var_def_op->GetRes()->SetType(entry->type_);
    GET_MLIR_CONTEXT()->GetResourceContext()->MapVarToValue(entry, var_def_op->GetRes());
    GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(var_def_op);
  } else {
    // If the variable is not an array, just get the value
    // Probably we should just regard it as a register, since it will not be transferred to a memory alloca
    if (var_def->init_values_) {
      auto init = static_cast<frontend::ast::InitValNode *>(var_def->init_values_.get());
      auto value = GET_MLIR_CONTEXT()->GetResourceContext()->GetValueFromExpr(init->value_.get());
      if (value == nullptr) {
        compiler_context_->ReportError(init->value_.get(), "Failed to get value from initializer expression.");
        return;
      }
      /**
       * @note: For array-variables, we need to check the type of the initials with the element type
       */
      auto type = entry->type_;
      if (type->GetKind() == type::TypeKind::ARRAY) {
        // In definition, type will only be array type instead of pointer
        type = static_cast<type::ArrayType *>(type)->GetElementType();
      }
      if (value->GetType() != type) {
        auto cast_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::CastOp>(GET_MLIR_CONTEXT(), value,
                                                                                 value->GetType(), type);
        value->AddUse(cast_op, 0);
        GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(cast_op);
        value = cast_op->GetRes();
      }
      // Add a copy operation to copy the value, since the value may be used in other places, and we don't want to
      // change the original value
      auto copy_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::CopyOp>(GET_MLIR_CONTEXT(), value);
      copy_op->GetRes()->SetType(value->GetType());
      GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(copy_op);
      value = copy_op->GetRes();
      GET_MLIR_CONTEXT()->GetResourceContext()->MapVarToValue(entry, value);
    } else {
      // Not initialized, provide a default value as `0` but different types
      mlir::arithm::ConstOp *default_op;
      // Then entry type should be integer or float.
      if (entry->type_->GetKind() == type::TypeKind::INT) {
        default_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::ConstOp>(GET_MLIR_CONTEXT(), 0);
        default_op->GetRes()->SetType(entry->type_);
      } else if (entry->type_->GetKind() == type::TypeKind::FLOAT) {
        default_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::ConstOp>(GET_MLIR_CONTEXT(), 0.0F);
        default_op->GetRes()->SetType(entry->type_);
      } else {
        compiler_context_->ReportError(var_def, "Unsupported variable type for default initializer.");
        return;
      }

      GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(default_op);
      GET_MLIR_CONTEXT()->GetResourceContext()->MapVarToValue(entry, default_op->GetRes());
    }
  }
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
  // First reset the constops recorded
  GET_MLIR_CONTEXT()->GetResourceContext()->Reset();
  std::vector<mlir::Value *> args;
  for (const auto &param : func_def->parameters_) {
    param->Accept(this);
    auto entry = GET_SYMBOLTABLE_CONTEXT()->GetVarEntryById(param->symbol_id_);
    auto value = GET_MLIR_CONTEXT()->GetResourceContext()->GetValueFromVar(entry);
    if (value == nullptr) {
      compiler_context_->ReportError(param.get(), "Failed to get value from function parameter.");
      return;
    }
    args.emplace_back(value);
  }

  auto func_def_op = GET_MLIR_CONTEXT()->CreateOperation<middleend::mlir::func::FuncDefOp>(
      GET_MLIR_CONTEXT(), func_def->func_name_, args, func_def->return_type_);

  // map func_name to the operation, used in function call
  GET_MLIR_CONTEXT()->GetResourceContext()->MapFuncToFuncDefOp(func_def->func_name_, func_def_op);

  GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(func_def_op);

  // set new insert point and current region to the block
  // in the region of the function definition operation,
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

  // If the function definition has no return statement, we can add a default return
  AddDefaultReturn(func_def_op);

  // Exit a funcdef, should get back to the module_op block
  auto top_region = GET_MLIR_CONTEXT()->GetModuleOp()->GetRegion();
  auto top_block = GET_MLIR_CONTEXT()->GetModuleOp()->GetBlock();
  GET_MLIR_CONTEXT()->SetInsertPoint(top_block);
  GET_MLIR_CONTEXT()->SetCurrentRegion(top_region);
  // Exit a funcdef, set current function definition to nullptr
  GET_MLIR_CONTEXT()->SetCurrentFuncDefOp(nullptr);
  // ConstOp cannot be shared between different functions
  GET_MLIR_CONTEXT()->GetResourceContext()->Reset();
}

}  // namespace middleend