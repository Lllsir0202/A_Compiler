#include "middleend/mlir/lower/lowerhelper.h"
namespace middleend::mlir::lower {
// Use macro to provide quick access to mlir context
#define GET_MLIR_CONTEXT() compiler_context_->GetIRContext()->GetMLIRContext()
#define GET_TYPE_CONTEXT() compiler_context_->GetTypeContext()
#define GET_SYMBOLTABLE_CONTEXT() compiler_context_->GetSymbolTableContext()

/**
 * @brief Use macro to dispatch the lowering
 */
#define LOWER_DISPATCH(OPERATION_TYPE, FUNC_NAME)             \
  if (auto specific_op = DynamicOrNull<OPERATION_TYPE>(op)) { \
    Lower##FUNC_NAME(specific_op);                            \
    return;                                                   \
  }

auto LowerHelper::LowerDispatch(Operation *op) -> void {
  // TODO(loki) : implement dispatching and lowering for different operations.
  // LOWER_DISPATCH(middleend::mlir::arithm::ConstOp, ConstOp);
  LOWER_DISPATCH(middleend::mlir::var::VarDefOp, VarDefOp);
}

/**
 * @brief Arithmetic operations, which would be separated into different operations
 * based on the types of the operands
 */

// Binary operations

// Separate the arithmetic operations based on the types into different base dialect operations

// Since the high level dialect has been through `verify`,
// which means the operands and result of the operation have been checked to be type-correct,
#define LOWER_BINARY_OP(OP_NAME)                                                                                    \
  auto LowerHelper::Lower##OP_NAME##Op(middleend::mlir::arithm::OP_NAME##Op *op)->void {                            \
    auto type = op->GetRes()->GetType();                                                                            \
    if (type->GetKind() == type::TypeKind::INT) {                                                                   \
      auto new_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::OP_NAME##Op>(GET_MLIR_CONTEXT(), op->GetRes(),  \
                                                                                 op->GetLHS(), op->GetRHS());       \
      op->GetLHS()->ReplaceAllOpUsesWith(op, new_op);                                                               \
      op->GetRHS()->ReplaceAllOpUsesWith(op, new_op);                                                               \
      GetInsertPoint()->AddOperation(new_op);                                                                       \
    } else if (type->GetKind() == type::TypeKind::FLOAT) {                                                          \
      auto new_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::OP_NAME##FOp>(GET_MLIR_CONTEXT(), op->GetRes(), \
                                                                                  op->GetLHS(), op->GetRHS());      \
      op->GetLHS()->ReplaceAllOpUsesWith(op, new_op);                                                               \
      op->GetRHS()->ReplaceAllOpUsesWith(op, new_op);                                                               \
      GetInsertPoint()->AddOperation(new_op);                                                                       \
    } else {                                                                                                        \
      compiler_context_->ReportError("Unsupported type for " #OP_NAME " operation: " + type->ToString());           \
    }                                                                                                               \
  }
LOWER_BINARY_OP(Add)
LOWER_BINARY_OP(Sub)
LOWER_BINARY_OP(Mul)
LOWER_BINARY_OP(Div)

auto LowerHelper::LowerModOp(middleend::mlir::arithm::ModOp *mod_op) -> void {
  // Similar to add operation, we separate the mod operation based on the types into different base dialect operations
  auto type = mod_op->GetRes()->GetType();
  if (type->GetKind() == type::TypeKind::INT) {
    auto modi_op_base = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::ModOp>(GET_MLIR_CONTEXT(), mod_op->GetRes(),
                                                                               mod_op->GetLHS(), mod_op->GetRHS());
    mod_op->GetLHS()->ReplaceAllOpUsesWith(mod_op, modi_op_base);
    mod_op->GetRHS()->ReplaceAllOpUsesWith(mod_op, modi_op_base);
    GetInsertPoint()->AddOperation(modi_op_base);
  } else {
    compiler_context_->ReportError("Unsupported type for mod operation: " + type->ToString());
  }
}

#define LOWER_LOGICAL_OP(OP_NAME)                                                                                   \
  auto LowerHelper::Lower##OP_NAME##Op(middleend::mlir::arithm::OP_NAME##Op *op)->void {                            \
    if (op->GetLHS()->GetType() != op->GetRHS()->GetType()) {                                                       \
      compiler_context_->ReportError(                                                                               \
          "Type mismatch for " #OP_NAME " operation: " + op->GetLHS()->GetType()->ToString() + " and " +            \
          op->GetRHS()->GetType()->ToString());                                                                     \
      return;                                                                                                       \
    }                                                                                                               \
    auto type = op->GetLHS()->GetType();                                                                            \
    if (type->GetKind() == type::TypeKind::INT) {                                                                   \
      auto new_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::OP_NAME##Op>(GET_MLIR_CONTEXT(), op->GetRes(),  \
                                                                                 op->GetLHS(), op->GetRHS());       \
      op->GetLHS()->ReplaceAllOpUsesWith(op, new_op);                                                               \
      op->GetRHS()->ReplaceAllOpUsesWith(op, new_op);                                                               \
      GetInsertPoint()->AddOperation(new_op);                                                                       \
    } else if (type->GetKind() == type::TypeKind::FLOAT) {                                                          \
      auto new_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::OP_NAME##FOp>(GET_MLIR_CONTEXT(), op->GetRes(), \
                                                                                  op->GetLHS(), op->GetRHS());      \
      op->GetLHS()->ReplaceAllOpUsesWith(op, new_op);                                                               \
      op->GetRHS()->ReplaceAllOpUsesWith(op, new_op);                                                               \
      GetInsertPoint()->AddOperation(new_op);                                                                       \
    } else {                                                                                                        \
      compiler_context_->ReportError("Unsupported type for " #OP_NAME " operation: " + type->ToString());           \
    }                                                                                                               \
  }
LOWER_LOGICAL_OP(Equal)
LOWER_LOGICAL_OP(NotEqual)
LOWER_LOGICAL_OP(Less)
LOWER_LOGICAL_OP(LessEqual)
LOWER_LOGICAL_OP(Greater)
LOWER_LOGICAL_OP(GreaterEqual)

// Unary operations
auto LowerHelper::LowerNegOp(middleend::mlir::arithm::NegOp *neg_op) -> void {
  auto type = neg_op->GetOperand()->GetType();
  if (type->GetKind() == type::TypeKind::INT) {
    auto new_neg_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::NegOp>(GET_MLIR_CONTEXT(), neg_op->GetRes(),
                                                                             neg_op->GetOperand());
    neg_op->GetOperand()->ReplaceAllOpUsesWith(neg_op, new_neg_op);
    GetInsertPoint()->AddOperation(new_neg_op);
  } else if (type->GetKind() == type::TypeKind::FLOAT) {
    auto new_neg_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::NegFOp>(GET_MLIR_CONTEXT(), neg_op->GetRes(),
                                                                              neg_op->GetOperand());
    neg_op->GetOperand()->ReplaceAllOpUsesWith(neg_op, new_neg_op);
    GetInsertPoint()->AddOperation(new_neg_op);
  } else {
    compiler_context_->ReportError("Unsupported type for neg operation: " + type->ToString());
  }
}

auto LowerHelper::LowerNotOp(middleend::mlir::arithm::NotOp *not_op) -> void {
  auto type = not_op->GetOperand()->GetType();
  // Not operation only accept boolean type,
  // other types will be transformed to boolean type

  /**
   * @note In the future, we should combine the cast and not part,
   * since we can directly cast the operand to not result, via comparsion operations
   */
  if (type->GetKind() == type::TypeKind::BOOL) {
    auto new_not_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::NotOp>(GET_MLIR_CONTEXT(), not_op->GetRes(),
                                                                             not_op->GetOperand());
    not_op->GetOperand()->ReplaceAllOpUsesWith(not_op, new_not_op);
    GetInsertPoint()->AddOperation(new_not_op);
  } else {
    compiler_context_->ReportError("Unsupported type for not operation: " + type->ToString());
  }
}

auto LowerHelper::LowerConstOp(middleend::mlir::arithm::ConstOp *const_op) -> void {
  // Similar to neg operation, we separate the const operation based on the types into different base dialect operations
  auto type = const_op->GetRes()->GetType();
  if (type->GetKind() == type::TypeKind::INT) {
    auto value = const_op->GetValue();
    if (!std::holds_alternative<int>(value)) {
      compiler_context_->ReportError("ConstOp: result type is not same as constant value\n");
      return;
    }
    auto new_const_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::ConstIntOp>(
        GET_MLIR_CONTEXT(), const_op->GetRes(), std::get<int>(value));
    GetInsertPoint()->AddOperation(new_const_op);
  } else if (type->GetKind() == type::TypeKind::FLOAT) {
    auto value = const_op->GetValue();
    if (!std::holds_alternative<float>(value)) {
      compiler_context_->ReportError("ConstOp: result type is not same as constant value\n");
      return;
    }
    auto new_const_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::ConstFloatOp>(
        GET_MLIR_CONTEXT(), const_op->GetRes(), std::get<float>(value));
    GetInsertPoint()->AddOperation(new_const_op);
  } else {
    compiler_context_->ReportError("Unsupported type for const operation: " + type->ToString());
  }
}

auto LowerHelper::LowerCastOp(middleend::mlir::arithm::CastOp *cast_op) -> void {
  // We need to separat the cast operation to specific cast operations
  auto from_type = cast_op->GetFromType();
  auto to_type = cast_op->GetToType();
  // INT 2 FLOAT
  if (from_type->GetKind() == type::TypeKind::INT && to_type->GetKind() == type::TypeKind::FLOAT) {
    auto new_cast_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::Int2FloatOp>(
        GET_MLIR_CONTEXT(), cast_op->GetRes(), cast_op->GetOperand());
    cast_op->GetOperand()->ReplaceAllOpUsesWith(cast_op, new_cast_op);
    GetInsertPoint()->AddOperation(new_cast_op);
  }
  // FLOAT 2 INT
  else if (from_type->GetKind() == type::TypeKind::FLOAT && to_type->GetKind() == type::TypeKind::INT) {
    auto new_cast_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::Float2IntOp>(
        GET_MLIR_CONTEXT(), cast_op->GetRes(), cast_op->GetOperand());
    cast_op->GetOperand()->ReplaceAllOpUsesWith(cast_op, new_cast_op);
    GetInsertPoint()->AddOperation(new_cast_op);
  }
  // BOOL 2 INT
  else if (from_type->GetKind() == type::TypeKind::BOOL && to_type->GetKind() == type::TypeKind::INT) {
    auto new_cast_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::Bool2IntOp>(
        GET_MLIR_CONTEXT(), cast_op->GetRes(), cast_op->GetOperand());
    cast_op->GetOperand()->ReplaceAllOpUsesWith(cast_op, new_cast_op);
    GetInsertPoint()->AddOperation(new_cast_op);
  }
  // INT 2 BOOL
  else if (from_type->GetKind() == type::TypeKind::INT && to_type->GetKind() == type::TypeKind::BOOL) {
    auto new_cast_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::Int2BoolOp>(
        GET_MLIR_CONTEXT(), cast_op->GetRes(), cast_op->GetOperand());
    cast_op->GetOperand()->ReplaceAllOpUsesWith(cast_op, new_cast_op);
    GetInsertPoint()->AddOperation(new_cast_op);
  }
  // BOOL 2 FLOAT
  else if (from_type->GetKind() == type::TypeKind::BOOL && to_type->GetKind() == type::TypeKind::FLOAT) {
    auto new_cast_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::Bool2FloatOp>(
        GET_MLIR_CONTEXT(), cast_op->GetRes(), cast_op->GetOperand());
    cast_op->GetOperand()->ReplaceAllOpUsesWith(cast_op, new_cast_op);
    GetInsertPoint()->AddOperation(new_cast_op);
  }
  // FLOAT 2 BOOL
  else if (from_type->GetKind() == type::TypeKind::FLOAT && to_type->GetKind() == type::TypeKind::BOOL) {
    auto new_cast_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::Float2BoolOp>(
        GET_MLIR_CONTEXT(), cast_op->GetRes(), cast_op->GetOperand());
    cast_op->GetOperand()->ReplaceAllOpUsesWith(cast_op, new_cast_op);
    GetInsertPoint()->AddOperation(new_cast_op);
  } else {
    compiler_context_->ReportError("Unsupported type for cast operation: from " + from_type->ToString() + " to " +
                                   to_type->ToString());
  }
}

/**
 * @brief Followings are the operations that are not directly lowered to base dialect,
 * includes: copy, var-dialect, func-dialect
 */

// Use macro
#define LOWER_CLONE_OP(DIALECT, OPERATION_TYPE)                                                 \
  auto LowerHelper::Lower##OPERATION_TYPE(middleend::mlir::DIALECT::OPERATION_TYPE *op)->void { \
    auto new_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::DIALECT::OPERATION_TYPE>(op);       \
    GetInsertPoint()->AddOperation(new_op);                                                     \
  }

LOWER_CLONE_OP(arithm, CopyOp)

// Variable-about operations
LOWER_CLONE_OP(var, VarDefOp)
LOWER_CLONE_OP(var, VarGetPtrOp)
LOWER_CLONE_OP(var, VarGetValOp)
LOWER_CLONE_OP(var, VarSetOp)

// Function-related operations
LOWER_CLONE_OP(func, FuncCallOp)
LOWER_CLONE_OP(func, ReturnOp)

// FuncDef is different, since it will contain a structured region
auto LowerHelper::LowerFuncDefOp(middleend::mlir::func::FuncDefOp *func_def_op) -> void {
  // We need to create a new function in the base dialect,
  // and then we need to lower the region in the function definition
  auto func_op_base = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::FuncDefOp>(
      func_def_op->GetRes(), func_def_op->GetName(), func_def_op->GetArguments());
  // Change all uses
  for (auto &arg : func_op_base->GetArguments()) {
    arg->ReplaceAllOpUsesWith(func_def_op, func_op_base);
  }
  GetInsertPoint()->AddOperation(func_op_base);

  SetCurrentRegion(func_op_base->regions_[0]);
  // Set entry block as the insert point
  auto entry_block = func_op_base->regions_[0]->blocks_[0];
  SetInsertPoint(entry_block);

  // Record current function
  SetCurrentFuncDefOp(func_op_base);

  // Then we need to lower the region in the function definition,
  // Since we are currently in the original block, which is the entry block of the function definition,
  // we can directly lower the operations in the region without changing the insert point,
  auto region = func_def_op->regions_[0];
  for (auto &block : region->blocks_) {
    for (auto &op : block->operations_) {
      LowerDispatch(op);
    }
  }

  // After lowering the region,
  // we need to reset the current region and insert point to nullptr,
  // as well as the current function definition operation
  SetCurrentRegion(nullptr);
  SetInsertPoint(nullptr);
  SetCurrentFuncDefOp(nullptr);
  ResetBlockID();
}

/**
 * @brief For scf operations, we need to do more work
 */
auto LowerHelper::LowerIfOp(middleend::mlir::scf::IfOp *if_op) -> void {
  // If Operation is a control flow operation in base dialect,
  // which is currently a structured control flow operation

  // We will only allow one region in base dialect,
  // which means we will just create all the blocks in the funcdef region
  auto then_region = if_op->GetThenRegion();
  auto else_region = if_op->GetElseRegion();
  // We need to lower the then region and else region separately,
  auto then_block = then_region->blocks_[0];
  auto else_block = else_region->blocks_[0];

  // Then we need to create different block, since we are currently in the original block,
  // which is the condition block
  auto condition_block = GetInsertPoint();
  // Then we should insert the brcondition to the end of condition block
  auto condition = if_op->GetCondition();
  auto br_cond_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::CondBrOp>(condition, then_block, else_block);
  // It is originally used by the IfOp
  condition->ReplaceAllOpUsesWith(if_op, br_cond_op);
  condition_block->AddOperation(br_cond_op);

  // Then we need to lower the then region and else region separately,

  // Deal with then block, insert new then block and new else block to current region
  auto new_then_block = new mlir::Block();
  auto new_else_block = new mlir::Block();

  // Need a merge block to merge the then block and else block
  auto merge_block = new mlir::Block();

  auto current_region = GetCurrentRegion();
  current_region->blocks_.emplace_back(new_then_block);
  current_region->blocks_.emplace_back(new_else_block);
  current_region->blocks_.emplace_back(merge_block);

  /**
   * @note: Since the YieldOp will only appear in the IfOp,
   * and the WhileOp, so we can directly deal with the YieldOp
   * in the lowering of IfOp and WhileOp,
   * instead of dealing with the YieldOp in the lowering of the region,
   * which is more general and would be more complicated, but it would be unnecessary.
   */

  // Then start with then block
  SetInsertPoint(new_then_block);
  for (auto &op : then_block->operations_) {
    LowerDispatch(op);
  }

  auto then_yield_op = DynamicOrNull<scf::YieldOp>(then_block->operations_.back());
  if (then_yield_op == nullptr) {
    compiler_context_->ReportError("Then block of IfOp must end with a YieldOp\n");
    return;
  }

  auto else_yield_op = DynamicOrNull<scf::YieldOp>(else_block->operations_.back());
  if (else_yield_op == nullptr) {
    compiler_context_->ReportError("Else block of IfOp must end with a YieldOp\n");
    return;
  }

  // After lowering the then block, we need to insert a branch to the merge block
  auto br_to_merge_from_then = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::BrOp>(merge_block);
  new_then_block->AddOperation(br_to_merge_from_then);

  // Then we need to lower the else block
  SetInsertPoint(new_else_block);
  for (auto &op : else_block->operations_) {
    LowerDispatch(op);
  }

  // Same as then block, add a branch to the merge block
  auto br_to_merge_from_else = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::BrOp>(merge_block);
  new_else_block->AddOperation(br_to_merge_from_else);

  // After lowering the then block and else block, we need to reset the insert point to the merge block
  SetInsertPoint(merge_block);

  // Also, we need to deal with the value merge in the merge block,
  // which means we need to create a new phi node in the merge block
  // to merge the values from then block and else block,
  // which in MLIR represents
  // as the yield operation in the then block and else block,
  // and the operand of the yield operation is the value to be merged

  if (then_yield_op->GetOperands().size() != else_yield_op->GetOperands().size()) {
    compiler_context_->ReportError("Then block and else block of IfOp must yield the same number of values\n");
    return;
  }
  auto size = then_yield_op->GetOperands().size();
  for (size_t i = 0; i < size; i++) {
    auto then_value = then_yield_op->GetOperands()[i];
    auto else_value = else_yield_op->GetOperands()[i];
    if (then_value->GetType() != else_value->GetType()) {
      compiler_context_->ReportError("Then block and else block of IfOp must yield the same type of values\n");
      return;
    }
    auto res = if_op->GetResultByIndex(i);
    // Since the mlir::Value* has been generated in high-level dialect, we only need to directly use them
    auto phi_node = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::PhiOp>(
        res, std::vector<mlir::Value *>{then_value, else_value},
        std::vector<mlir::Block *>{new_then_block, new_else_block});

    then_value->ReplaceAllOpUsesWith(then_yield_op, phi_node, 0);
    else_value->ReplaceAllOpUsesWith(else_yield_op, phi_node, 1);
    GetInsertPoint()->AddOperation(phi_node);
  }

  // Finish lowering the IfOp
}

auto LowerHelper::LowerWhileOp(middleend::mlir::scf::WhileOp *while_op) -> void {
  // While operation has cond region and body region,
  // cond region is ended with a `condition` operation
  // body region is ended with a `yield` operation
  
}

}  // namespace middleend::mlir::lower