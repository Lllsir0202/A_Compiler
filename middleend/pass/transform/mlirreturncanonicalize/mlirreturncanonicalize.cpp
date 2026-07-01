#include "middleend/pass/transform/mlirreturncanonicalize/mlirreturncanonicalize.h"
#include <vector>
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/func/funcdef.h"
#include "middleend/mlir/dialect/func/return.h"
#include "middleend/mlir/dialect/scf/if.h"
#include "middleend/mlir/dialect/scf/yield.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/block.h"
#include "middleend/mlir/utils/struct/region.h"
#include "middleend/mlir/utils/value/value.h"

#define GET_MLIR_CONTEXT() compiler_context_->GetIRContext()->GetMLIRContext()

namespace middleend::pass::transform {
auto MLIRReturnCanonicalizePass::Run() -> void {
  bool changed = false;
  auto *module_op = GET_MLIR_CONTEXT()->GetModuleOp();
  if (module_op == nullptr) {
    throw std::logic_error("Module operation is not set in MLIRContext");
  }
  for (auto *operation : module_op->GetBlock()->operations_) {
    auto *func_op = mlir::DynamicOrNull<mlir::func::FuncDefOp>(operation);
    if (func_op == nullptr) {
      continue;
    }
    changed |= RunOnFunction(func_op);
  }
  if (!changed) {
    return;
  }
  for (auto *operation : module_op->GetBlock()->operations_) {
    auto *func_op = mlir::DynamicOrNull<mlir::func::FuncDefOp>(operation);
    if (func_op != nullptr) {
      RebuildUses(func_op);
    }
  }
}

auto MLIRReturnCanonicalizePass::RunOnFunction(mlir::func::FuncDefOp *func_op) -> bool {
  auto *block = func_op->GetEntryBlock();
  if (block->operations_.empty()) {
    return false;
  }
  bool changed = false;
  while (true) {
    auto *final_return = mlir::DynamicOrNull<mlir::func::ReturnOp>(block->operations_.back());
    if (final_return == nullptr || final_return->GetReturnValue() == nullptr) {
      return changed;
    }
    bool local_changed = false;
    // Right-to-left: prevents earlier IfOps from being buried in nested else-blocks before they're processed.
    for (int i = (int)block->operations_.size() - 2; i >= 0; --i) {
      auto *if_op = mlir::DynamicOrNull<mlir::scf::IfOp>(block->operations_[(size_t)i]);
      if (if_op == nullptr) {
        continue;
      }
      if (RewriteEarlyReturnIf(func_op, block, (size_t)i, if_op)) {
        changed = true;
        local_changed = true;
        break;
      }
    }
    if (!local_changed) {
      return changed;
    }
  }
}

auto MLIRReturnCanonicalizePass::IsSimpleEarlyReturnThen(mlir::scf::IfOp *if_op,
                                                         mlir::func::ReturnOp **return_op) -> bool {
  auto *then_region = if_op->GetThenRegion();
  auto *else_region = if_op->GetElseRegion();
  if (then_region == nullptr || else_region == nullptr || then_region->blocks_.size() != 1 ||
      else_region->blocks_.size() != 1) {
    return false;
  }
  auto *then_block = then_region->blocks_[0];
  auto *else_block = else_region->blocks_[0];
  if (then_block->operations_.size() < 2 || else_block->operations_.size() != 1) {
    return false;
  }
  auto *then_return =
      mlir::DynamicOrNull<mlir::func::ReturnOp>(then_block->operations_[then_block->operations_.size() - 2]);
  auto *then_yield = mlir::DynamicOrNull<mlir::scf::YieldOp>(then_block->operations_.back());
  auto *else_yield = mlir::DynamicOrNull<mlir::scf::YieldOp>(else_block->operations_.back());
  if (then_return == nullptr || then_yield == nullptr || else_yield == nullptr ||
      then_return->GetReturnValue() == nullptr) {
    return false;
  }
  *return_op = then_return;
  return true;
}

auto MLIRReturnCanonicalizePass::IsBothBranchReturn(mlir::scf::IfOp *if_op, mlir::func::ReturnOp **then_return,
                                                    mlir::func::ReturnOp **else_return) -> bool {
  auto *then_region = if_op->GetThenRegion();
  auto *else_region = if_op->GetElseRegion();
  if (then_region == nullptr || else_region == nullptr || then_region->blocks_.size() != 1 ||
      else_region->blocks_.size() != 1) {
    return false;
  }
  auto *then_block = then_region->blocks_[0];
  auto *else_block = else_region->blocks_[0];
  if (then_block->operations_.size() < 2 || else_block->operations_.size() < 2) {
    return false;
  }
  auto *t_ret = mlir::DynamicOrNull<mlir::func::ReturnOp>(then_block->operations_[then_block->operations_.size() - 2]);
  auto *t_yield = mlir::DynamicOrNull<mlir::scf::YieldOp>(then_block->operations_.back());
  auto *e_ret = mlir::DynamicOrNull<mlir::func::ReturnOp>(else_block->operations_[else_block->operations_.size() - 2]);
  auto *e_yield = mlir::DynamicOrNull<mlir::scf::YieldOp>(else_block->operations_.back());
  if (t_ret == nullptr || t_yield == nullptr || t_ret->GetReturnValue() == nullptr || e_ret == nullptr ||
      e_yield == nullptr || e_ret->GetReturnValue() == nullptr) {
    return false;
  }
  *then_return = t_ret;
  *else_return = e_ret;
  return true;
}

auto MLIRReturnCanonicalizePass::RewriteEarlyReturnIf(mlir::func::FuncDefOp *func_op, mlir::Block *block,
                                                      size_t if_index, mlir::scf::IfOp *if_op) -> bool {
  mlir::func::ReturnOp *then_return = nullptr;
  if (!IsSimpleEarlyReturnThen(if_op, &then_return)) {
    // Try the both-branch-return pattern: both then and else end with a return.
    mlir::func::ReturnOp *else_return = nullptr;
    if (!IsBothBranchReturn(if_op, &then_return, &else_return)) {
      return false;
    }

    auto *then_block = if_op->GetThenRegion()->blocks_[0];
    auto *else_block = if_op->GetElseRegion()->blocks_[0];
    auto *then_val = then_return->GetReturnValue();
    auto *else_val = else_return->GetReturnValue();

    then_block->operations_.erase(then_block->operations_.end() - 2);
    auto *then_yield = mlir::DynamicOrNull<mlir::scf::YieldOp>(then_block->operations_.back());
    then_yield->GetOperands().clear();
    then_yield->GetOperands().emplace_back(then_val);

    else_block->operations_.erase(else_block->operations_.end() - 2);
    auto *else_yield = mlir::DynamicOrNull<mlir::scf::YieldOp>(else_block->operations_.back());
    else_yield->GetOperands().clear();
    else_yield->GetOperands().emplace_back(else_val);

    auto *new_if_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::IfOp>(
        GET_MLIR_CONTEXT(), 1, if_op->GetCondition(), if_op->GetThenRegion(), if_op->GetElseRegion());
    new_if_op->GetResultByIndex(0)->SetType(func_op->GetReturnType());
    if_op->regions_.clear();

    auto *new_return = GET_MLIR_CONTEXT()->CreateOperation<mlir::func::ReturnOp>(
        GET_MLIR_CONTEXT(), func_op->GetReturnType(), new_if_op->GetResultByIndex(0));

    block->operations_.resize(if_index);
    block->operations_.emplace_back(new_if_op);
    block->operations_.emplace_back(new_return);
    return true;
  }

  std::vector<mlir::Operation *> suffix_operations;
  suffix_operations.reserve(block->operations_.size() - if_index - 1);
  for (size_t i = if_index + 1; i < block->operations_.size(); ++i) {
    suffix_operations.emplace_back(block->operations_[i]);
  }

  auto *else_block = if_op->GetElseRegion()->blocks_[0];
  else_block->operations_.pop_back();
  for (auto *operation : suffix_operations) {
    else_block->operations_.emplace_back(operation);
  }

  auto *else_return = mlir::DynamicOrNull<mlir::func::ReturnOp>(else_block->operations_.back());
  if (else_return == nullptr || else_return->GetReturnValue() == nullptr) {
    return false;
  }

  auto *then_block = if_op->GetThenRegion()->blocks_[0];
  then_block->operations_.erase(then_block->operations_.end() - 2);
  auto *then_yield = mlir::DynamicOrNull<mlir::scf::YieldOp>(then_block->operations_.back());
  then_yield->GetOperands().clear();
  then_yield->GetOperands().emplace_back(then_return->GetReturnValue());

  else_block->operations_.pop_back();
  auto *else_yield =
      GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::YieldOp>(GET_MLIR_CONTEXT(), else_return->GetReturnValue());
  else_block->operations_.emplace_back(else_yield);

  auto *new_if_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::IfOp>(
      GET_MLIR_CONTEXT(), 1, if_op->GetCondition(), if_op->GetThenRegion(), if_op->GetElseRegion());
  new_if_op->GetResultByIndex(0)->SetType(func_op->GetReturnType());
  if_op->regions_.clear();

  auto *new_return = GET_MLIR_CONTEXT()->CreateOperation<mlir::func::ReturnOp>(
      GET_MLIR_CONTEXT(), func_op->GetReturnType(), new_if_op->GetResultByIndex(0));

  block->operations_.resize(if_index);
  block->operations_.emplace_back(new_if_op);
  block->operations_.emplace_back(new_return);
  return true;
}

auto MLIRReturnCanonicalizePass::RebuildUsesInOperation(mlir::Operation *operation) -> void {
  for (size_t i = 0; i < operation->operands_.size(); ++i) {
    auto *operand = operation->operands_[i];
    if (operand != nullptr) {
      operand->AddUse(operation, i);
    }
  }
  for (auto *region : operation->regions_) {
    for (auto *block : region->blocks_) {
      for (auto *nested_op : block->operations_) {
        RebuildUsesInOperation(nested_op);
      }
    }
  }
}

// Clearing must cover the whole function (including nested block arguments and
// results) before any rebuild starts: a while-loop's cond and body blocks share
// the same loop-carried block argument value, so clearing a region after a
// sibling region was already rebuilt would wipe the uses that sibling added.
auto MLIRReturnCanonicalizePass::ClearUsesInRegion(mlir::Region *region) -> void {
  for (auto *block : region->blocks_) {
    for (auto *argument : block->arguments_) {
      if (argument != nullptr) {
        argument->ClearUses();
      }
    }
    for (auto *operation : block->operations_) {
      for (auto *result : operation->results_) {
        if (result != nullptr) {
          result->ClearUses();
        }
      }
      for (auto *nested_region : operation->regions_) {
        ClearUsesInRegion(nested_region);
      }
    }
  }
}

auto MLIRReturnCanonicalizePass::RebuildUses(mlir::func::FuncDefOp *func_op) -> void {
  // Phase 1: clear every use in the function before rebuilding any.
  for (auto *argument : func_op->GetArguments()) {
    argument->ClearUses();
  }
  for (auto *region : func_op->regions_) {
    ClearUsesInRegion(region);
  }
  // Phase 2: rebuild operand uses now that all use-lists start empty.
  for (auto *region : func_op->regions_) {
    for (auto *block : region->blocks_) {
      for (auto *operation : block->operations_) {
        RebuildUsesInOperation(operation);
      }
    }
  }
}

}  // namespace middleend::pass::transform
