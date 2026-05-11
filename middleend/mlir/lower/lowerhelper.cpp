#include "middleend/mlir/lower/lowerhelper.h"
#include <exception>
#include "mlir/dialect/base/phi.h"
#include "mlir/dialect/func/funccall.h"
#include "mlir/dialect/func/funcdef.h"
#include "mlir/dialect/func/return.h"
#include "mlir/dialect/scf/inits.h"
#include "mlir/dialect/scf/while.h"
#include "mlir/dialect/var/def.h"
#include "mlir/dialect/var/getptr.h"
#include "mlir/dialect/var/getvalue.h"
#include "mlir/dialect/var/globaldef.h"
#include "mlir/dialect/var/set.h"
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
  LOWER_DISPATCH(mlir::arithm::AddOp, AddOp)
  LOWER_DISPATCH(mlir::arithm::SubOp, SubOp)
  LOWER_DISPATCH(mlir::arithm::MulOp, MulOp)
  LOWER_DISPATCH(mlir::arithm::DivOp, DivOp)
  LOWER_DISPATCH(mlir::arithm::ModOp, ModOp)
  LOWER_DISPATCH(mlir::arithm::EqualOp, EqualOp)
  LOWER_DISPATCH(mlir::arithm::NotEqualOp, NotEqualOp)
  LOWER_DISPATCH(mlir::arithm::LessOp, LessOp)
  LOWER_DISPATCH(mlir::arithm::LessEqualOp, LessEqualOp)
  LOWER_DISPATCH(mlir::arithm::GreaterOp, GreaterOp)
  LOWER_DISPATCH(mlir::arithm::GreaterEqualOp, GreaterEqualOp)
  LOWER_DISPATCH(mlir::arithm::NegOp, NegOp)
  LOWER_DISPATCH(mlir::arithm::NotOp, NotOp)
  LOWER_DISPATCH(mlir::arithm::ConstOp, ConstOp)
  LOWER_DISPATCH(mlir::arithm::CastOp, CastOp)
  LOWER_DISPATCH(mlir::arithm::CopyOp, CopyOp)
  LOWER_DISPATCH(mlir::func::FuncDefOp, FuncDefOp)
  LOWER_DISPATCH(mlir::func::FuncCallOp, FuncCallOp)
  LOWER_DISPATCH(mlir::func::ReturnOp, ReturnOp)
  LOWER_DISPATCH(mlir::var::VarDefOp, VarDefOp)
  LOWER_DISPATCH(mlir::var::GlobalVarDefOp, GlobalVarDefOp)
  LOWER_DISPATCH(mlir::var::VarGetPtrOp, VarGetPtrOp)
  LOWER_DISPATCH(mlir::var::VarGetValOp, VarGetValOp)
  LOWER_DISPATCH(mlir::var::VarSetOp, VarSetOp)
  LOWER_DISPATCH(mlir::scf::WhileOp, WhileOp)
  LOWER_DISPATCH(mlir::scf::BreakOp, BreakOp)
  LOWER_DISPATCH(mlir::scf::ContinueOp, ContinueOp)
  LOWER_DISPATCH(mlir::scf::ConditionOp, ConditionOp)
  LOWER_DISPATCH(mlir::scf::YieldOp, YieldOp)
  LOWER_DISPATCH(mlir::scf::IfOp, IfOp)
  LOWER_DISPATCH(mlir::scf::InitsOp, InitsOp)
  compiler_context_->ReportError("Unsupported operation for lowering: " + op->Name());
}

/**
 * @brief Arithmetic operations, which would be separated into different operations
 * based on the types of the operands
 */

// Binary operations

// Separate the arithmetic operations based on the types into different base dialect operations

// Since the high level dialect has been through `verify`,
// which means the operands and result of the operation have been checked to be type-correct,
#define LOWER_BINARY_OP(OP_NAME)                                                                                   \
  auto LowerHelper::Lower##OP_NAME##Op(middleend::mlir::arithm::OP_NAME##Op *op)->void {                           \
    auto type = op->GetRes()->GetType();                                                                           \
    if (type->GetKind() == type::TypeKind::INT) {                                                                  \
      auto new_op =                                                                                                \
          GET_MLIR_CONTEXT()->CreateOperation<mlir::base::OP_NAME##Op>(op->GetRes(), op->GetLHS(), op->GetRHS());  \
      op->GetLHS()->ReplaceAllOpUsesWith(op, new_op);                                                              \
      op->GetRHS()->ReplaceAllOpUsesWith(op, new_op);                                                              \
      GetInsertPoint()->AddOperation(new_op);                                                                      \
    } else if (type->GetKind() == type::TypeKind::FLOAT) {                                                         \
      auto new_op =                                                                                                \
          GET_MLIR_CONTEXT()->CreateOperation<mlir::base::OP_NAME##FOp>(op->GetRes(), op->GetLHS(), op->GetRHS()); \
      op->GetLHS()->ReplaceAllOpUsesWith(op, new_op);                                                              \
      op->GetRHS()->ReplaceAllOpUsesWith(op, new_op);                                                              \
      GetInsertPoint()->AddOperation(new_op);                                                                      \
    } else {                                                                                                       \
      compiler_context_->ReportError("Unsupported type for " #OP_NAME " operation: " + type->ToString());          \
    }                                                                                                              \
  }
LOWER_BINARY_OP(Add)
LOWER_BINARY_OP(Sub)
LOWER_BINARY_OP(Mul)
LOWER_BINARY_OP(Div)

auto LowerHelper::LowerModOp(middleend::mlir::arithm::ModOp *mod_op) -> void {
  // Similar to add operation, we separate the mod operation based on the types into different base dialect operations
  auto type = mod_op->GetRes()->GetType();
  if (type->GetKind() == type::TypeKind::INT) {
    auto modi_op_base =
        GET_MLIR_CONTEXT()->CreateOperation<mlir::base::ModOp>(mod_op->GetRes(), mod_op->GetLHS(), mod_op->GetRHS());
    mod_op->GetLHS()->ReplaceAllOpUsesWith(mod_op, modi_op_base);
    mod_op->GetRHS()->ReplaceAllOpUsesWith(mod_op, modi_op_base);
    GetInsertPoint()->AddOperation(modi_op_base);
  } else {
    compiler_context_->ReportError("Unsupported type for mod operation: " + type->ToString());
  }
}

#define LOWER_LOGICAL_OP(OP_NAME)                                                                                  \
  auto LowerHelper::Lower##OP_NAME##Op(middleend::mlir::arithm::OP_NAME##Op *op)->void {                           \
    if (op->GetLHS()->GetType() != op->GetRHS()->GetType()) {                                                      \
      compiler_context_->ReportError(                                                                              \
          "Type mismatch for " #OP_NAME " operation: " + op->GetLHS()->GetType()->ToString() + " and " +           \
          op->GetRHS()->GetType()->ToString());                                                                    \
      return;                                                                                                      \
    }                                                                                                              \
    auto type = op->GetLHS()->GetType();                                                                           \
    if (type->GetKind() == type::TypeKind::INT) {                                                                  \
      auto new_op =                                                                                                \
          GET_MLIR_CONTEXT()->CreateOperation<mlir::base::OP_NAME##Op>(op->GetRes(), op->GetLHS(), op->GetRHS());  \
      op->GetLHS()->ReplaceAllOpUsesWith(op, new_op);                                                              \
      op->GetRHS()->ReplaceAllOpUsesWith(op, new_op);                                                              \
      GetInsertPoint()->AddOperation(new_op);                                                                      \
    } else if (type->GetKind() == type::TypeKind::FLOAT) {                                                         \
      auto new_op =                                                                                                \
          GET_MLIR_CONTEXT()->CreateOperation<mlir::base::OP_NAME##FOp>(op->GetRes(), op->GetLHS(), op->GetRHS()); \
      op->GetLHS()->ReplaceAllOpUsesWith(op, new_op);                                                              \
      op->GetRHS()->ReplaceAllOpUsesWith(op, new_op);                                                              \
      GetInsertPoint()->AddOperation(new_op);                                                                      \
    } else {                                                                                                       \
      compiler_context_->ReportError("Unsupported type for " #OP_NAME " operation: " + type->ToString());          \
    }                                                                                                              \
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
    auto new_neg_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::NegOp>(neg_op->GetRes(), neg_op->GetOperand());
    neg_op->GetOperand()->ReplaceAllOpUsesWith(neg_op, new_neg_op);
    GetInsertPoint()->AddOperation(new_neg_op);
  } else if (type->GetKind() == type::TypeKind::FLOAT) {
    auto new_neg_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::NegFOp>(neg_op->GetRes(), neg_op->GetOperand());
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
    auto new_not_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::NotOp>(not_op->GetRes(), not_op->GetOperand());
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
    auto new_const_op =
        GET_MLIR_CONTEXT()->CreateOperation<mlir::base::ConstIntOp>(const_op->GetRes(), std::get<int>(value));
    GetInsertPoint()->AddOperation(new_const_op);
  } else if (type->GetKind() == type::TypeKind::FLOAT) {
    auto value = const_op->GetValue();
    if (!std::holds_alternative<float>(value)) {
      compiler_context_->ReportError("ConstOp: result type is not same as constant value\n");
      return;
    }
    auto new_const_op =
        GET_MLIR_CONTEXT()->CreateOperation<mlir::base::ConstFloatOp>(const_op->GetRes(), std::get<float>(value));
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
    auto new_cast_op =
        GET_MLIR_CONTEXT()->CreateOperation<mlir::base::Int2FloatOp>(cast_op->GetRes(), cast_op->GetOperand());
    cast_op->GetOperand()->ReplaceAllOpUsesWith(cast_op, new_cast_op);
    GetInsertPoint()->AddOperation(new_cast_op);
  }
  // FLOAT 2 INT
  else if (from_type->GetKind() == type::TypeKind::FLOAT && to_type->GetKind() == type::TypeKind::INT) {
    auto new_cast_op =
        GET_MLIR_CONTEXT()->CreateOperation<mlir::base::Float2IntOp>(cast_op->GetRes(), cast_op->GetOperand());
    cast_op->GetOperand()->ReplaceAllOpUsesWith(cast_op, new_cast_op);
    GetInsertPoint()->AddOperation(new_cast_op);
  }
  // BOOL 2 INT
  else if (from_type->GetKind() == type::TypeKind::BOOL && to_type->GetKind() == type::TypeKind::INT) {
    auto new_cast_op =
        GET_MLIR_CONTEXT()->CreateOperation<mlir::base::Bool2IntOp>(cast_op->GetRes(), cast_op->GetOperand());
    cast_op->GetOperand()->ReplaceAllOpUsesWith(cast_op, new_cast_op);
    GetInsertPoint()->AddOperation(new_cast_op);
  }
  // INT 2 BOOL
  else if (from_type->GetKind() == type::TypeKind::INT && to_type->GetKind() == type::TypeKind::BOOL) {
    auto new_cast_op =
        GET_MLIR_CONTEXT()->CreateOperation<mlir::base::Int2BoolOp>(cast_op->GetRes(), cast_op->GetOperand());
    cast_op->GetOperand()->ReplaceAllOpUsesWith(cast_op, new_cast_op);
    GetInsertPoint()->AddOperation(new_cast_op);
  }
  // BOOL 2 FLOAT
  else if (from_type->GetKind() == type::TypeKind::BOOL && to_type->GetKind() == type::TypeKind::FLOAT) {
    auto new_cast_op =
        GET_MLIR_CONTEXT()->CreateOperation<mlir::base::Bool2FloatOp>(cast_op->GetRes(), cast_op->GetOperand());
    cast_op->GetOperand()->ReplaceAllOpUsesWith(cast_op, new_cast_op);
    GetInsertPoint()->AddOperation(new_cast_op);
  }
  // FLOAT 2 BOOL
  else if (from_type->GetKind() == type::TypeKind::FLOAT && to_type->GetKind() == type::TypeKind::BOOL) {
    auto new_cast_op =
        GET_MLIR_CONTEXT()->CreateOperation<mlir::base::Float2BoolOp>(cast_op->GetRes(), cast_op->GetOperand());
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

// Variable-about operations
LOWER_CLONE_OP(var, VarDefOp)
LOWER_CLONE_OP(var, GlobalVarDefOp)
LOWER_CLONE_OP(var, VarGetPtrOp)
LOWER_CLONE_OP(var, VarGetValOp)
LOWER_CLONE_OP(var, VarSetOp)

// Function-related operations
LOWER_CLONE_OP(func, ReturnOp)

auto LowerHelper::LowerCopyOp(middleend::mlir::arithm::CopyOp *copy_op) -> void {
  // Copy operation is a special case, since it is a high level operation that does not exist in the base dialect,
  // we will just clone the copy operation to the base dialect, and then we will lower it in the next step
  auto new_copy_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::CopyOp>(copy_op->GetRes(), copy_op->GetValue());
  GetInsertPoint()->AddOperation(new_copy_op);
}

auto LowerHelper::LowerFuncCallOp(middleend::mlir::func::FuncCallOp *func_call_op) -> void {
  auto new_func_call_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::FuncCallOp>(
      func_call_op->GetRes(), func_call_op->GetName(), func_call_op->GetOperands());
  GetInsertPoint()->AddOperation(new_func_call_op);
}

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

  // Record current region and block
  auto current_region = GetCurrentRegion();
  auto current_block = GetInsertPoint();

  SetCurrentRegion(func_op_base->regions_[0]);
  // Set entry block as the insert point
  auto entry_block = func_op_base->regions_[0]->blocks_[0];
  SetInsertPoint(entry_block);

  // Record current function
  SetCurrentFuncDefOp(func_op_base);

  // Record map
  GET_MLIR_CONTEXT()->GetLowerContext()->MapFuncNameToFuncDefOp(func_op_base->GetName(), func_op_base);

  // Then we need to lower the region in the function definition,
  // Since we are currently in the original block, which is the entry block of the function definition,
  // we can directly lower the operations in the region without changing the insert point,
  auto region = func_def_op->regions_[0];
  for (auto &block : region->blocks_) {
    for (auto &op : block->operations_) {
      try {
        LowerDispatch(op);
      } catch (std::exception &e) {
        compiler_context_->ReportError("Error occurred while lowering operation: " + std::string(e.what()));
      }
    }
  }

  // After lowering the region,
  // we need to reset the current region and insert point to nullptr,
  // as well as the current function definition operation
  SetCurrentRegion(current_region);
  SetInsertPoint(current_block);
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

  // Then we need to lower the then region and else region separately,

  // Deal with then block, insert new then block and new else block to current region
  auto new_then_block = new mlir::Block();
  auto then_prefix = GetCurrentFuncDefOp()->GetName() + "_then";
  SetUniqueBlockID(new_then_block, then_prefix);
  auto new_else_block = new mlir::Block();
  auto else_prefix = GetCurrentFuncDefOp()->GetName() + "_else";
  SetUniqueBlockID(new_else_block, else_prefix);

  // Need a merge block to merge the then block and else block
  auto merge_block = new mlir::Block();
  auto merge_prefix = GetCurrentFuncDefOp()->GetName() + "_if_merge";
  SetUniqueBlockID(merge_block, merge_prefix);

  // Then we should insert the brcondition to the end of condition block
  auto condition = if_op->GetCondition();
  auto br_cond_op =
      GET_MLIR_CONTEXT()->CreateOperation<mlir::base::CondBrOp>(condition, new_then_block, new_else_block);
  // It is originally used by the IfOp
  condition->ReplaceAllOpUsesWith(if_op, br_cond_op);
  condition_block->AddOperation(br_cond_op);

  auto current_region = GetCurrentRegion();
  current_region->blocks_.emplace_back(new_then_block);
  current_region->blocks_.emplace_back(new_else_block);
  current_region->blocks_.emplace_back(merge_block);

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

  // After lowering the then block, we need to insert a branch to the merge block
  /**
   * @note: Since there might be recursive IfOp or WhileOp in the then block,
   * we cannot directly insert the branch to the merge block
   */
  auto br_to_merge_from_then = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::BrOp>(merge_block);
  GetInsertPoint()->AddOperation(br_to_merge_from_then);
  // Br from then block to merge block, which will be used in phi
  auto control_flow_from_then = GetInsertPoint();

  // Then we need to lower the else block
  SetInsertPoint(new_else_block);
  for (auto &op : else_block->operations_) {
    LowerDispatch(op);
  }

  // Same as then block, add a branch to the merge block
  auto br_to_merge_from_else = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::BrOp>(merge_block);
  GetInsertPoint()->AddOperation(br_to_merge_from_else);
  // Br from else block to merge block, which will be used in phi
  auto control_flow_from_else = GetInsertPoint();

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
        std::vector<mlir::Block *>{control_flow_from_then, control_flow_from_else});

    then_value->ReplaceAllOpUsesWith(then_yield_op, phi_node, 0);
    else_value->ReplaceAllOpUsesWith(else_yield_op, phi_node, 1);
    GetInsertPoint()->AddOperation(phi_node);
  }

  // Finish lowering the IfOp, need to jump back from the merge block to the new original block
  auto new_block = new mlir::Block();
  auto prefix = GetCurrentFuncDefOp()->GetName() + "_if_continue";
  SetUniqueBlockID(new_block, prefix);
  GetCurrentRegion()->blocks_.emplace_back(new_block);
  auto br_to_new_block = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::BrOp>(new_block);
  merge_block->AddOperation(br_to_new_block);
  SetInsertPoint(new_block);
}

auto LowerHelper::LowerWhileOp(middleend::mlir::scf::WhileOp *while_op) -> void {
  // Enter a loop
  GET_MLIR_CONTEXT()->GetLowerContext()->EnterWhileLoop(while_op);

  // While operation has cond region and body region,
  // cond region is ended with a `condition` operation
  // body region is ended with a `yield` operation
  auto cond_region = while_op->GetCondRegion();
  auto body_region = while_op->GetBodyRegion();

  // Get cond and region block
  auto cond_block = cond_region->blocks_[0];
  auto body_block = body_region->blocks_[0];

  // Current block is the block before the while loop, need to br to cond block
  auto current_block = GetInsertPoint();

  // Create new blocks for cond, body and merge
  auto new_cond_block = new mlir::Block();
  auto cond_prefix = GetCurrentFuncDefOp()->GetName() + "_while_cond";
  SetUniqueBlockID(new_cond_block, cond_prefix);

  auto new_body_block = new mlir::Block();
  auto body_prefix = GetCurrentFuncDefOp()->GetName() + "_while_body";
  SetUniqueBlockID(new_body_block, body_prefix);

  auto exit_block = new mlir::Block();
  auto exit_prefix = GetCurrentFuncDefOp()->GetName() + "_while_exit";
  SetUniqueBlockID(exit_block, exit_prefix);

  auto current_region = GetCurrentRegion();
  current_region->blocks_.emplace_back(new_cond_block);
  current_region->blocks_.emplace_back(new_body_block);
  current_region->blocks_.emplace_back(exit_block);

  // Handle the condition operation and yield operation in whileop
  auto condition_op = DynamicOrNull<scf::ConditionOp>(cond_block->operations_.back());
  if (condition_op == nullptr) {
    compiler_context_->ReportError("Condition block of WhileOp must end with a ConditionOp\n");
    return;
  }

  auto yield_op = DynamicOrNull<scf::YieldOp>(body_block->operations_.back());
  if (yield_op == nullptr) {
    compiler_context_->ReportError("Body block of WhileOp must end with a YieldOp\n");
    return;
  }

  // Since the `phi` should be in the head of the block, we deal with them first.
  /**
   * @brief condition operation should decide whether to branch to body block or exit block
   * If cond:
   * true: means while(true), branch to body block
   * false: means while(false), branch to exit block
   * @note note that the condition operation need to update the value via phi node,
   * since we don't allow the value updated in the condition block
   * -- just because there is no operation that can update the value in an expression
   * but to be more clear, might we should record the values before entering the while loop,
   * and then directly use them as the operand when we jump from current block to cond block,
   * instead of using the operand of condition operation, so that it might cause less bugs,
   * once we allow the value to be updated in an expression.
   * so if :
   * jump from current block(means from outside of the while loop) to cond block,
   * then the value is the initial value, which is the operand of the condition operation
   * if:
   * jump from body block to cond block, means we are in the while loop, then the value is the updated value,
   * which is the operand of the yield operation in body block

   * And we only need to deal with the values which changed during the loop,
   * since there is no need to merge the values which are not changed during the loop
   */
  auto condition_operands = condition_op->GetOperands();
  auto condition = condition_op->GetCondition();
  auto yield_operands = yield_op->GetOperands();
  // Deal with the original operands
  auto original_operands = condition_op->GetOriginalValues();

  if (condition_operands.size() != yield_operands.size()) {
    compiler_context_->ReportError(
        "Condition operation and yield operation of WhileOp must have the same number of operands\n");
    return;
  }
  if (condition_operands.size() != original_operands.size()) {
    compiler_context_->ReportError(
        "Condition operation and original values\n of WhileOp must have the same number of operands: " +
        std::to_string(condition_operands.size()) + " and " + std::to_string(original_operands.size()));
    return;
  }

  auto size = condition_operands.size();
  // Start with jumping from current block to condition block
  // if from current block, then phi is original value, otherwise, phi is the value from yield

  // Create phi node and fill the incomings later
  std::vector<mlir::base::PhiOp *> phi_nodes;

  auto cond_block_args = cond_block->arguments_;
  auto body_block_args = body_block->arguments_;
  if (cond_block_args.size() != size || body_block_args.size() != size) {
    compiler_context_->ReportError(
        "Condition block and body block of WhileOp must have the same number of arguments as the operands of "
        "condition operation and yield operation\n");
    return;
  }

  // If from current block, then phi is original value, otherwise, phi is the value from yield
  for (size_t i = 0; i < size; i++) {
    auto result = cond_block_args[i];

    // If from current block, then phi is original value,
    // otherwise, phi is the value from yield
    auto phi_node = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::PhiOp>(result);
    phi_nodes.emplace_back(phi_node);
    // Phi node should be added to the head of the condition block,
    // which means it should be added before the condition block lowering
    new_cond_block->AddOperation(phi_node);
  }

  /**
   * @brief Since the exit block will decide what value to choose,
   * for instance:
   * @example
   * while (x < 10) {
   *   x = x + 1
   * }
   * in this case, since we only allow exit the loop from the condition block,
   * we only need to consider the value from condition block, which is the args passed by block.
   * BUT, if we import break operation, then we need to consider the value from body block,
   * we can create a new phi node in the exit block to merge the values from condition block and body block,
   * and in optimization, we erase the phi node which has only one incoming edge, since it is redundant.
   */
  for (size_t i = 0; i < size; i++) {
    auto result = while_op->GetResultByIndex(i);
    // In exit block, most of the time, the value is from condition block.
    auto phi_node = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::PhiOp>(result);
    phi_node->AddIncoming(cond_block_args[i], new_cond_block);
    exit_block->AddOperation(phi_node);
  }

  // We can map whileop to exit_block, so that when we meet `break`,
  //  we will be able to deal with it correctly.
  GET_MLIR_CONTEXT()->GetLowerContext()->MapWhileOpToExitBlock(while_op, exit_block);
  GET_MLIR_CONTEXT()->GetLowerContext()->MapWhileOpToCondBlock(while_op, new_cond_block);
  GET_MLIR_CONTEXT()->GetLowerContext()->MapWhileOpToBodyBlock(while_op, new_body_block);

  // Add branch from current block to cond block
  auto br_to_cond = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::BrOp>(new_cond_block);
  current_block->AddOperation(br_to_cond);

  // Then we need to lower the cond block and body block separately,
  // Start with cond block
  SetInsertPoint(new_cond_block);
  for (auto &op : cond_block->operations_) {
    LowerDispatch(op);
  }

  // Since currently, the phi node will only be added in the condition block,
  // because all branch will only jump to the condition block, and then jump to the exit block

  // Since the condition block will always jump from current block to condition block,
  // so we don't need to consider the scf change the br operation's `from`.
  auto br_cond_to_body =
      GET_MLIR_CONTEXT()->CreateOperation<mlir::base::CondBrOp>(condition, new_body_block, exit_block);
  GetInsertPoint()->AddOperation(br_cond_to_body);

  // Since the condition operation has been handled, we can directly pop back

  // Same as IfOp, we will deal with the condition operation in whileop

  SetInsertPoint(new_body_block);
  for (auto &op : body_block->operations_) {
    LowerDispatch(op);
  }
  auto br_body_to_cond = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::BrOp>(new_cond_block);
  GetInsertPoint()->AddOperation(br_body_to_cond);
  auto control_flow_from_body = GetInsertPoint();

  // Set the incomings of phi nodes
  for (size_t i = 0; i < size; i++) {
    auto phi_node = phi_nodes[i];
    auto original_value = original_operands[i];
    auto yield_value = yield_operands[i];
    // If from current block, then phi is original value,
    // otherwise, phi is the value from yield
    phi_node->AddIncoming(original_value, current_block);
    phi_node->AddIncoming(yield_value, control_flow_from_body);
    original_value->ReplaceAllOpUsesWith(condition_op, phi_node, 0);
    yield_value->ReplaceAllOpUsesWith(yield_op, phi_node, 1);
  }

  // Same as IfOp, we will deal with the yield operation in whileop

  // After lowering the cond block and body block, we need to set the insert point to the exit block
  SetInsertPoint(exit_block);

  // Exit the loop
  GET_MLIR_CONTEXT()->GetLowerContext()->ExitWhileLoop();
}

auto LowerHelper::LowerBreakOp(middleend::mlir::scf::BreakOp *break_op) -> void {
  // Since the break operation will directly jump to the exit block of the while loop,
  // and we need to update the phi node in the exit block, which means we need to add a incoming
  // from the break operation to the exit block
  auto current_loop = GET_MLIR_CONTEXT()->GetLowerContext()->GetCurrentWhileLoop();
  if (current_loop == nullptr) {
    compiler_context_->ReportError("Break operation must be inside a while loop\n");
    return;
  }
  auto exit_block = GET_MLIR_CONTEXT()->GetLowerContext()->GetExitBlockFromWhileOp(current_loop);
  // Add a branch from current block to exit block
  auto current_block = GetInsertPoint();
  auto br_to_exit = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::BrOp>(exit_block);
  current_block->AddOperation(br_to_exit);

  // If break operation has operands, which is current state of the loop, we need to update the phi node in the exit
  // block
  auto size = current_loop->GetResultsSize();
  auto operands = break_op->GetOperands();
  if (operands.size() != size) {
    compiler_context_->ReportError(
        "Break operation must have the same number of operands as the while operation's results: " +
        std::to_string(operands.size()) + " and " + std::to_string(size));
    return;
  }

  // Update the phi node in the exit block
  for (size_t i = 0; i < size; i++) {
    // Implementation for updating phi nodes
    auto operand = operands[i];
    auto phi_node = exit_block->operations_[i];
    if (auto phi_op = DynamicOrNull<mlir::base::PhiOp>(phi_node)) {
      phi_op->AddIncoming(operand, current_block);
      operand->ReplaceAllOpUsesWith(break_op, phi_op, phi_op->GetIncoming().size() - 1);
    } else {
      compiler_context_->ReportError("The operation in the exit block is not a PhiOp\n");
      return;
    }
  }
}

auto LowerHelper::LowerContinueOp([[maybe_unused]] middleend::mlir::scf::ContinueOp *continue_op) -> void {
  /**
   * @brief For continue operation, it will directly jump to the condition block of the while loop,
   * and we need to update the phi node in the condition block,
   * which means we need to add a incoming from the continue operation to the condition block
   */
  auto current_loop = GET_MLIR_CONTEXT()->GetLowerContext()->GetCurrentWhileLoop();
  if (current_loop == nullptr) {
    compiler_context_->ReportError("Continue operation must be inside a while loop\n");
    return;
  }
  auto cond_block = GET_MLIR_CONTEXT()->GetLowerContext()->GetCondBlockFromWhileOp(current_loop);
  // Add a branch from current block to condition block
  auto current_block = GetInsertPoint();
  auto br_to_cond = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::BrOp>(cond_block);
  current_block->AddOperation(br_to_cond);
  // Then add incoming to the phi node in the condition block
  auto operands = continue_op->GetOperands();
  auto size = current_loop->GetResultsSize();
  if (operands.size() != size) {
    compiler_context_->ReportError(
        "Continue operation must have the same number of operands as the while operation's results: " +
        std::to_string(operands.size()) + " and " + std::to_string(size));
    return;
  }

  // Update the phi node in the condition block
  for (size_t i = 0; i < size; i++) {
    auto operand = operands[i];
    auto phi_node = cond_block->operations_[i];
    if (auto phi_op = DynamicOrNull<mlir::base::PhiOp>(phi_node)) {
      phi_op->AddIncoming(operand, current_block);
      operand->ReplaceAllOpUsesWith(continue_op, phi_op, phi_op->GetIncoming().size() - 1);
    } else {
      compiler_context_->ReportError("The operation in the condition block is not a PhiOp\n");
      return;
    }
  }
}

auto LowerHelper::LowerConditionOp([[maybe_unused]] middleend::mlir::scf::ConditionOp *condition_op) -> void {}

auto LowerHelper::LowerYieldOp([[maybe_unused]] middleend::mlir::scf::YieldOp *yield_op) -> void {}

auto LowerHelper::LowerInitsOp(middleend::mlir::scf::InitsOp *inits_op) -> void {
  GetInsertPoint()->AddOperation(inits_op);
}

}  // namespace middleend::mlir::lower