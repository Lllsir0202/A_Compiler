#include "middleend/builder/mlir/mlirvisitor.h"
#include "ast/ast_expr.h"
#include "ast/ast_utils.h"
#include "frontend/ast/ast_spec.h"
#include "middleend/mlir/dialect/arithm/arithm.h"
#include "middleend/mlir/dialect/var/var.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/region.h"
#include "mlir/dialect/var/def.h"
#include "mlir/dialect/var/get.h"
#include "mlir/utils/value/value.h"
#include "utils/flatten/mlirinit.h"
#define GET_MLIR_CONTEXT() compiler_context_->GetIRContext()->GetMLIRContext()
#define GET_TYPE_CONTEXT() compiler_context_->GetTypeContext()
#define GET_SYMBOLTABLE_CONTEXT() compiler_context_->GetSymbolTableContext()
namespace middleend {

#include "middleend/builder/mlir/binary_handler.def"
/**
 * @brief The Helper functions for handing out operators
 */
inline auto MLIRVisitor::HandleBinaryOperator(frontend::ast::BinaryExprNode *binary_expr,
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
inline auto MLIRVisitor::HandleUnaryOperator(frontend::ast::UnaryExprNode *unary_expr,
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
  root->Accept(this);
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
  /**
   * @brief we use the map to get value from the ExprNode*,
   * since every expression node will ONLY generate one value,
   * so we can just use the expression node as the key to get the value from the map, and then we can use the value to
   * build the operation for this binary expression
   */
  auto res = HandleBinaryOperator(binary_expr, compiler_context_);
  GET_MLIR_CONTEXT()->GetResourceContext()->MapExprToValue(binary_expr, res);
}
void MLIRVisitor::Visit(frontend::ast::UnaryExprNode *unary_expr) {
  auto operand = unary_expr->operand_.get();
  operand->Accept(this);
  /**
   * @brief the operand has been visited,
   * and the result of the operand shall be stored in the MLIR StateContext,
   * also it should be the top value */
  auto res = HandleUnaryOperator(unary_expr, compiler_context_);
  GET_MLIR_CONTEXT()->GetResourceContext()->MapExprToValue(unary_expr, res);
}
void MLIRVisitor::Visit([[maybe_unused]] frontend::ast::FuncCallExprNode *func_call) {}
void MLIRVisitor::Visit(frontend::ast::LValExprNode *lval_expr) {
  /**
   * @brief The lval expression shall be a variable
   */
  auto symbol_id = lval_expr->symbol_id_;
  auto entry = GET_SYMBOLTABLE_CONTEXT()->GetVarEntryById(symbol_id);
  if (entry == nullptr) {
    compiler_context_->ReportError(lval_expr, "variable is not defined");
    return;
  }
  // Since the variable is defined, we can get its value from the symbol table entry
  auto ptr = GET_MLIR_CONTEXT()->GetResourceContext()->GetValueFromVar(entry);
  // if the variable is a array
  if (entry->dimensions_.empty()) {
    // Not an array variable, we can directly get the value from the symbol table entry
    auto var_get_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarGetOp>(GET_MLIR_CONTEXT(), ptr);
    GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(var_get_op);
    GET_MLIR_CONTEXT()->GetResourceContext()->MapExprToValue(lval_expr, var_get_op->GetRes());
    auto type = entry->type_;
    var_get_op->GetRes()->SetType(type);
  } else {
    // Is an array
    auto indices = std::vector<mlir::Value *>{};
    for (const auto &index : lval_expr->indices_) {
      index->Accept(this);
      auto index_value = GET_MLIR_CONTEXT()->GetResourceContext()->GetValueFromExpr(index.get());
      indices.emplace_back(index_value);
    }
    auto var_get_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarGetOp>(GET_MLIR_CONTEXT(), ptr, indices);
    GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(var_get_op);
    GET_MLIR_CONTEXT()->GetResourceContext()->MapExprToValue(lval_expr, var_get_op->GetRes());
    auto type = entry->type_;
    if (type->GetKind() == type::TypeKind::ARRAY) {
      // if it is an array variable, we need to get the element type of the array type
      type = static_cast<type::ArrayType *>(type)->GetElementType();
    } else if (type->GetKind() == type::TypeKind::PTR) {
      // if it is a pointer variable, we need to get the base type of the pointer type
      type = static_cast<type::PtrType *>(type)->GetBaseType();
    }
    var_get_op->GetRes()->SetType(type);
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
  for (const auto &stmt : block_stmt->block_items_) {
    stmt->Accept(this);
  }
}
void MLIRVisitor::Visit(frontend::ast::AssignStmtNode *assign_stmt) {
  auto &lval = assign_stmt->lval_;
  auto &expr = assign_stmt->expr_;
  lval->Accept(this);
  expr->Accept(this);
  auto ptr = GET_MLIR_CONTEXT()->GetResourceContext()->GetValueFromExpr(lval.get());
  auto value = GET_MLIR_CONTEXT()->GetResourceContext()->GetValueFromExpr(expr.get());
  // Create a var.set operation to set the value of the variables
  auto var_set_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarSetOp>(GET_MLIR_CONTEXT(), ptr, value);
  GET_MLIR_CONTEXT()->GetInsertPoint()->AddOperation(var_set_op);
  // It should not generate any value, even though I set the result
}
void MLIRVisitor::Visit([[maybe_unused]] frontend::ast::ExprStmtNode *expr_stmt) {}
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
void MLIRVisitor::Visit([[maybe_unused]] frontend::ast::GlobalVarDeclNode *global_var_decl) {
  for (const auto &var_def : global_var_decl->defs_) {
    var_def->Accept(this);
  }
}
void MLIRVisitor::Visit([[maybe_unused]] frontend::ast::LocalVarDeclNode *local_var_decl) {
  for (const auto &var_def : local_var_decl->defs_) {
    var_def->Accept(this);
  }
}
void MLIRVisitor::Visit([[maybe_unused]] frontend::ast::InitListNode *init_list) {
  for (const auto &init_val : init_list->init_values_) {
    init_val->Accept(this);
  }
}
void MLIRVisitor::Visit([[maybe_unused]] frontend::ast::InitValNode *init_val) { init_val->value_->Accept(this); }
// Function nodes
void MLIRVisitor::Visit([[maybe_unused]] frontend::ast::FuncFParamNode *func_fparam) {}
void MLIRVisitor::Visit([[maybe_unused]] frontend::ast::FuncDefNode *func_def) {}

}  // namespace middleend