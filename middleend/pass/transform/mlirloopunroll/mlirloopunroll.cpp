#include "middleend/pass/transform/mlirloopunroll/mlirloopunroll.h"
#include <algorithm>
#include <cstdint>
#include <vector>
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/func/funcdef.h"
#include "middleend/mlir/dialect/func/return.h"
#include "middleend/mlir/dialect/scf/break.h"
#include "middleend/mlir/dialect/scf/condition.h"
#include "middleend/mlir/dialect/scf/continue.h"
#include "middleend/mlir/dialect/scf/if.h"
#include "middleend/mlir/dialect/scf/while.h"
#include "middleend/mlir/dialect/scf/yield.h"
#include "middleend/mlir/utils/clone/uselistrebuilder.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/block.h"
#include "middleend/mlir/utils/struct/region.h"
#include "middleend/mlir/utils/value/value.h"
#include "middleend/pass/transform/mlirloopunroll/inductionvariable.h"

#define GET_MLIR_CONTEXT() compiler_context_->GetIRContext()->GetMLIRContext()

namespace middleend::pass::transform {

auto MLIRLoopUnrollPass::Run() -> void {
  auto *module_op = GET_MLIR_CONTEXT()->GetModuleOp();
  if (module_op == nullptr) {
    throw std::logic_error("Module operation is not set in MLIRContext");
  }
  for (auto *operation : module_op->GetBlock()->operations_) {
    auto *func_op = mlir::DynamicOrNull<mlir::func::FuncDefOp>(operation);
    if (func_op == nullptr) {
      continue;
    }
    auto *entry = func_op->GetEntryBlock();
    if (entry == nullptr || entry->operations_.empty()) {
      continue;
    }
    RunOnFunction(func_op);
  }
}

auto MLIRLoopUnrollPass::RunOnFunction(mlir::func::FuncDefOp *func_op) -> bool {
  bool changed = false;
  for (size_t round = 0; round < kMaxRounds; ++round) {
    // ReplaceAllUsesWith on the loop results below needs current use-lists, and previous rounds
    // spliced cloned operations without registering their uses; rebuild before each attempt.
    mlir::utils::RebuildFunctionUses(func_op);
    WhileSite site;
    if (!FindUnrollableWhile(func_op->GetEntryBlock(), &site)) {
      break;
    }
    FullyUnroll(func_op, site);
    changed = true;
  }
  if (changed) {
    mlir::utils::RebuildFunctionUses(func_op);
  }
  return changed;
}

auto MLIRLoopUnrollPass::FindUnrollableWhile(mlir::Block *block, WhileSite *site) -> bool {
  for (size_t index = 0; index < block->operations_.size(); ++index) {
    auto *operation = block->operations_[index];
    if (auto *while_op = mlir::DynamicOrNull<mlir::scf::WhileOp>(operation)) {
      // Innermost-first: an inner counted loop is unrolled before its enclosing loop, which then
      // becomes eligible once its body no longer holds a while.
      for (auto *region : {while_op->GetCondRegion(), while_op->GetBodyRegion()}) {
        for (auto *inner : region->blocks_) {
          if (FindUnrollableWhile(inner, site)) {
            return true;
          }
        }
      }
      int64_t trip_count = 0;
      if (CanFullyUnroll(while_op, &trip_count)) {
        site->block = block;
        site->index = index;
        site->while_op = while_op;
        site->trip_count = trip_count;
        return true;
      }
      continue;
    }
    if (auto *if_op = mlir::DynamicOrNull<mlir::scf::IfOp>(operation)) {
      for (auto *region : {if_op->GetThenRegion(), if_op->GetElseRegion()}) {
        for (auto *inner : region->blocks_) {
          if (FindUnrollableWhile(inner, site)) {
            return true;
          }
        }
      }
    }
  }
  return false;
}

auto MLIRLoopUnrollPass::CanFullyUnroll(mlir::scf::WhileOp *while_op, int64_t *trip_count) -> bool {
  if (!loopiv::BodyIsCloneable(while_op->GetBodyRegion())) {
    return false;
  }
  int64_t trip = 0;
  if (!ComputeTripCount(while_op, &trip)) {
    return false;
  }
  auto per_iteration = static_cast<int64_t>(loopiv::CountOperations(while_op->GetBodyRegion()));
  if (trip * per_iteration > kMaxUnrollCost) {
    return false;
  }
  *trip_count = trip;
  return true;
}

auto MLIRLoopUnrollPass::ComputeTripCount(mlir::scf::WhileOp *while_op, int64_t *trip_count) -> bool {
  auto iv = loopiv::Recognize(while_op);
  int64_t trip = loopiv::ConstantTripCount(iv, kMaxTripCount);
  if (trip < 0) {
    return false;
  }
  *trip_count = trip;
  return true;
}

auto MLIRLoopUnrollPass::FullyUnroll(mlir::func::FuncDefOp *func_op, const WhileSite &site) -> void {
  auto *while_op = site.while_op;
  auto *cond_block = while_op->GetCondRegion()->blocks_[0];
  auto *body_block = while_op->GetBodyRegion()->blocks_[0];
  auto *condition_op = mlir::DynamicOrNull<mlir::scf::ConditionOp>(cond_block->operations_.back());
  auto *yield_op = mlir::DynamicOrNull<mlir::scf::YieldOp>(body_block->operations_.back());
  size_t size = cond_block->arguments_.size();

  // Values entering the current iteration; iteration 0 starts from the loop's original inits.
  std::vector<mlir::Value *> carried = condition_op->GetOriginalValues();

  std::vector<mlir::Operation *> unrolled;
  for (int64_t iteration = 0; iteration < site.trip_count; ++iteration) {
    cloner_.value_map_.clear();
    for (size_t i = 0; i < size; ++i) {
      cloner_.value_map_[cond_block->arguments_[i]] = carried[i];
    }
    // Clone every body operation except the trailing yield.
    for (size_t index = 0; index + 1 < body_block->operations_.size(); ++index) {
      unrolled.emplace_back(cloner_.Clone(body_block->operations_[index]));
    }
    std::vector<mlir::Value *> next;
    next.reserve(size);
    for (size_t i = 0; i < size; ++i) {
      next.emplace_back(cloner_.RemapValue(yield_op->GetOperands()[i]));
    }
    carried = std::move(next);
  }

  // After the (proven) final iteration the loop-carried values that failed the condition become
  // the loop results.
  std::unordered_map<mlir::Value *, mlir::Value *> substitutions;
  for (size_t i = 0; i < size; ++i) {
    auto *result = while_op->GetResultByIndex(i);
    result->ReplaceAllUsesWith(carried[i]);
    substitutions[result] = carried[i];
  }
  for (auto *region : func_op->regions_) {
    PatchConditionInits(region, substitutions);
  }

  // Replace the while operation with the unrolled straight-line body in its containing block.
  std::vector<mlir::Operation *> new_operations;
  new_operations.reserve(site.block->operations_.size() - 1 + unrolled.size());
  for (size_t i = 0; i < site.index; ++i) {
    new_operations.emplace_back(site.block->operations_[i]);
  }
  for (auto *operation : unrolled) {
    new_operations.emplace_back(operation);
  }
  for (size_t i = site.index + 1; i < site.block->operations_.size(); ++i) {
    new_operations.emplace_back(site.block->operations_[i]);
  }
  site.block->operations_ = std::move(new_operations);
}

auto MLIRLoopUnrollPass::PatchConditionInits(mlir::Region *region,
                                             const std::unordered_map<mlir::Value *, mlir::Value *> &subs) -> void {
  for (auto *block : region->blocks_) {
    for (auto *operation : block->operations_) {
      if (operation == nullptr) {
        continue;
      }
      if (auto *condition = mlir::DynamicOrNull<mlir::scf::ConditionOp>(operation)) {
        auto originals = condition->GetOriginalValues();
        for (size_t i = 0; i < originals.size(); ++i) {
          auto iter = subs.find(originals[i]);
          if (iter != subs.end()) {
            condition->UpdateOriginalValue(i, iter->second);
          }
        }
      }
      for (auto *nested : operation->regions_) {
        PatchConditionInits(nested, subs);
      }
    }
  }
}

}  // namespace middleend::pass::transform
