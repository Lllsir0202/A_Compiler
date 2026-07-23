#include "middleend/mlir/utils/clone/operationcloner.h"
#include <stdexcept>
#include <variant>
#include <vector>
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/arithm/binaryop.h"
#include "middleend/mlir/dialect/arithm/copy.h"
#include "middleend/mlir/dialect/arithm/unaryop.h"
#include "middleend/mlir/dialect/func/funccall.h"
#include "middleend/mlir/dialect/func/return.h"
#include "middleend/mlir/dialect/scf/condition.h"
#include "middleend/mlir/dialect/scf/if.h"
#include "middleend/mlir/dialect/scf/while.h"
#include "middleend/mlir/dialect/scf/yield.h"
#include "middleend/mlir/dialect/var/def.h"
#include "middleend/mlir/dialect/var/getptr.h"
#include "middleend/mlir/dialect/var/getvalue.h"
#include "middleend/mlir/dialect/var/set.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/block.h"
#include "middleend/mlir/utils/struct/region.h"
#include "middleend/mlir/utils/value/value.h"

#define GET_MLIR_CONTEXT() compiler_context_->GetIRContext()->GetMLIRContext()

namespace middleend::mlir::utils {

auto OperationCloner::CloneResult(mlir::Value *original) -> mlir::Value * {
  if (original == nullptr) {
    return nullptr;
  }
  auto *result = GET_MLIR_CONTEXT()->CreateValue();
  result->SetType(original->GetType());
  result->name_ = original->name_;
  value_map_[original] = result;
  return result;
}

auto OperationCloner::RemapValue(mlir::Value *value) -> mlir::Value * {
  if (value == nullptr) {
    return nullptr;
  }
  auto iter = value_map_.find(value);
  if (iter != value_map_.end()) {
    return iter->second;
  }
  return value;
}

auto OperationCloner::CloneBlock(mlir::Block *block) -> mlir::Block * {
  auto *new_block = new mlir::Block();
  new_block->block_name_ = block->block_name_;
  new_block->block_id_ = block->block_id_;
  for (auto *operation : block->operations_) {
    new_block->operations_.emplace_back(Clone(operation));
  }
  return new_block;
}

auto OperationCloner::CloneRegion(mlir::Region *region) -> mlir::Region * {
  auto *new_region = new mlir::Region();
  for (auto *block : region->blocks_) {
    new_region->blocks_.emplace_back(CloneBlock(block));
  }
  return new_region;
}

auto OperationCloner::Clone(mlir::Operation *operation) -> mlir::Operation * {
  if (operation == nullptr) {
    throw std::logic_error("OperationCloner cannot clone a null operation");
  }
  if (auto *call_op = mlir::DynamicOrNull<mlir::func::FuncCallOp>(operation)) {
    std::vector<mlir::Value *> operands;
    for (auto *operand : call_op->GetOperands()) {
      operands.emplace_back(RemapValue(operand));
    }
    auto *new_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::func::FuncCallOp>(GET_MLIR_CONTEXT(), call_op->GetName(),
                                                                               operands, call_op->GetRes()->GetType());
    value_map_[call_op->GetRes()] = new_op->GetRes();
    return new_op;
  }
  if (auto *return_op = mlir::DynamicOrNull<mlir::func::ReturnOp>(operation)) {
    if (return_op->GetReturnValue() != nullptr) {
      return GET_MLIR_CONTEXT()->CreateOperation<mlir::func::ReturnOp>(
          GET_MLIR_CONTEXT(), return_op->GetRes()->GetType(), RemapValue(return_op->GetReturnValue()));
    }
    return GET_MLIR_CONTEXT()->CreateOperation<mlir::func::ReturnOp>(GET_MLIR_CONTEXT(),
                                                                     return_op->GetRes()->GetType());
  }
  if (auto *copy_op = mlir::DynamicOrNull<mlir::arithm::CopyOp>(operation)) {
    auto *new_op =
        GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::CopyOp>(GET_MLIR_CONTEXT(), RemapValue(copy_op->GetValue()));
    new_op->GetRes()->SetType(copy_op->GetRes()->GetType());
    value_map_[copy_op->GetRes()] = new_op->GetRes();
    return new_op;
  }
  if (auto *const_op = mlir::DynamicOrNull<mlir::arithm::ConstOp>(operation)) {
    mlir::arithm::ConstOp *new_op = nullptr;
    auto value = const_op->GetValue();
    if (std::holds_alternative<int>(value)) {
      new_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::ConstOp>(GET_MLIR_CONTEXT(), std::get<int>(value));
    } else {
      new_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::ConstOp>(GET_MLIR_CONTEXT(), std::get<float>(value));
    }
    new_op->GetRes()->SetType(const_op->GetRes()->GetType());
    value_map_[const_op->GetRes()] = new_op->GetRes();
    return new_op;
  }
  if (auto *cast_op = mlir::DynamicOrNull<mlir::arithm::CastOp>(operation)) {
    auto *new_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::CastOp>(
        GET_MLIR_CONTEXT(), RemapValue(cast_op->GetOperand()), cast_op->GetFromType(), cast_op->GetToType());
    value_map_[cast_op->GetRes()] = new_op->GetRes();
    return new_op;
  }
  if (auto *var_def_op = mlir::DynamicOrNull<mlir::var::VarDefOp>(operation)) {
    std::vector<mlir::Value *> inits;
    for (auto *init : var_def_op->GetInits()) {
      inits.emplace_back(RemapValue(init));
    }
    auto *new_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarDefOp>(
        GET_MLIR_CONTEXT(), std::vector<int>(var_def_op->GetDimensions().begin(), var_def_op->GetDimensions().end()),
        inits);
    new_op->GetRes()->SetType(var_def_op->GetRes()->GetType());
    value_map_[var_def_op->GetRes()] = new_op->GetRes();
    return new_op;
  }
  if (auto *get_ptr_op = mlir::DynamicOrNull<mlir::var::VarGetPtrOp>(operation)) {
    std::vector<mlir::Value *> indices;
    for (auto *index : get_ptr_op->GetIndices()) {
      indices.emplace_back(RemapValue(index));
    }
    auto *new_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarGetPtrOp>(
        GET_MLIR_CONTEXT(), RemapValue(get_ptr_op->GetPtr()), indices);
    new_op->GetRes()->SetType(get_ptr_op->GetRes()->GetType());
    value_map_[get_ptr_op->GetRes()] = new_op->GetRes();
    return new_op;
  }
  if (auto *get_val_op = mlir::DynamicOrNull<mlir::var::VarGetValOp>(operation)) {
    auto *new_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarGetValOp>(GET_MLIR_CONTEXT(),
                                                                               RemapValue(get_val_op->GetPtr()));
    new_op->GetRes()->SetType(get_val_op->GetRes()->GetType());
    value_map_[get_val_op->GetRes()] = new_op->GetRes();
    return new_op;
  }
  if (auto *set_op = mlir::DynamicOrNull<mlir::var::VarSetOp>(operation)) {
    auto *new_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarSetOp>(
        GET_MLIR_CONTEXT(), RemapValue(set_op->GetPtr()), RemapValue(set_op->GetNewValue()));
    new_op->GetRes()->SetType(set_op->GetRes()->GetType());
    value_map_[set_op->GetRes()] = new_op->GetRes();
    return new_op;
  }
  if (auto *yield_op = mlir::DynamicOrNull<mlir::scf::YieldOp>(operation)) {
    std::vector<mlir::Value *> operands;
    for (auto *operand : yield_op->GetOperands()) {
      operands.emplace_back(RemapValue(operand));
    }
    return GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::YieldOp>(GET_MLIR_CONTEXT(), operands);
  }
  if (auto *condition_op = mlir::DynamicOrNull<mlir::scf::ConditionOp>(operation)) {
    std::unordered_map<symboltable::VarEntry *, mlir::Value *> empty_originals;
    auto *new_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::ConditionOp>(
        GET_MLIR_CONTEXT(), RemapValue(condition_op->GetCondition()), empty_originals);
    std::vector<mlir::Value *> values;
    for (auto *value : condition_op->GetOperands()) {
      values.emplace_back(RemapValue(value));
    }
    std::vector<mlir::Value *> original_values;
    for (auto *value : condition_op->GetOriginalValues()) {
      original_values.emplace_back(RemapValue(value));
    }
    new_op->SetLoopCarriedValues(values, original_values);
    return new_op;
  }
  if (auto *if_op = mlir::DynamicOrNull<mlir::scf::IfOp>(operation)) {
    auto *new_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::IfOp>(
        GET_MLIR_CONTEXT(), if_op->results_.size(), RemapValue(if_op->GetCondition()),
        CloneRegion(if_op->GetThenRegion()), CloneRegion(if_op->GetElseRegion()));
    for (size_t i = 0; i < if_op->results_.size(); ++i) {
      new_op->GetResultByIndex(i)->SetType(if_op->GetResultByIndex(i)->GetType());
      value_map_[if_op->GetResultByIndex(i)] = new_op->GetResultByIndex(i);
    }
    return new_op;
  }
  if (auto *while_op = mlir::DynamicOrNull<mlir::scf::WhileOp>(operation)) {
    // The scf.while cond and body blocks share the SAME loop-carried block-argument Value
    // objects. Clone each shared argument exactly once and register it in the value map BEFORE
    // cloning the regions, so references inside both regions remap to the cloned argument; then
    // attach the same cloned arguments to both cloned blocks to preserve the shared invariant.
    auto *cond_block = while_op->GetCondRegion()->blocks_[0];
    std::vector<mlir::Value *> cloned_args;
    cloned_args.reserve(cond_block->arguments_.size());
    for (auto *arg : cond_block->arguments_) {
      cloned_args.emplace_back(CloneResult(arg));
    }
    auto *new_cond_region = CloneRegion(while_op->GetCondRegion());
    auto *new_body_region = CloneRegion(while_op->GetBodyRegion());
    for (auto *cloned_arg : cloned_args) {
      new_cond_region->blocks_[0]->AddArgument(cloned_arg);
      new_body_region->blocks_[0]->AddArgument(cloned_arg);
    }
    auto *new_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::WhileOp>(
        GET_MLIR_CONTEXT(), while_op->GetResultsSize(), new_cond_region, new_body_region);
    for (size_t i = 0; i < while_op->GetResultsSize(); ++i) {
      new_op->GetResultByIndex(i)->SetType(while_op->GetResultByIndex(i)->GetType());
      value_map_[while_op->GetResultByIndex(i)] = new_op->GetResultByIndex(i);
    }
    return new_op;
  }

#define CLONE_BINARY_OP(OP_TYPE)                                                               \
  if (auto *binary_op = mlir::DynamicOrNull<mlir::arithm::OP_TYPE##Op>(operation)) {           \
    auto *new_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::OP_TYPE##Op>(             \
        GET_MLIR_CONTEXT(), RemapValue(binary_op->GetLHS()), RemapValue(binary_op->GetRHS())); \
    new_op->GetRes()->SetType(binary_op->GetRes()->GetType());                                 \
    value_map_[binary_op->GetRes()] = new_op->GetRes();                                        \
    return new_op;                                                                             \
  }

  CLONE_BINARY_OP(Add)
  CLONE_BINARY_OP(Sub)
  CLONE_BINARY_OP(Mul)
  CLONE_BINARY_OP(Div)
  CLONE_BINARY_OP(Mod)
  CLONE_BINARY_OP(Equal)
  CLONE_BINARY_OP(NotEqual)
  CLONE_BINARY_OP(Less)
  CLONE_BINARY_OP(LessEqual)
  CLONE_BINARY_OP(Greater)
  CLONE_BINARY_OP(GreaterEqual)

#undef CLONE_BINARY_OP

#define CLONE_UNARY_OP(OP_TYPE)                                                                                        \
  if (auto *unary_op = mlir::DynamicOrNull<mlir::arithm::OP_TYPE##Op>(operation)) {                                    \
    auto *new_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::OP_TYPE##Op>(GET_MLIR_CONTEXT(),                  \
                                                                                  RemapValue(unary_op->GetOperand())); \
    new_op->GetRes()->SetType(unary_op->GetRes()->GetType());                                                          \
    value_map_[unary_op->GetRes()] = new_op->GetRes();                                                                 \
    return new_op;                                                                                                     \
  }

  CLONE_UNARY_OP(Neg)
  CLONE_UNARY_OP(Not)

#undef CLONE_UNARY_OP

  throw std::logic_error("Unsupported operation in OperationCloner: " + operation->Name());
}

}  // namespace middleend::mlir::utils
