#include "middleend/pass/transform/mlirloopunroll/mlirloopunroll.h"
#include <algorithm>
#include <cstdint>
#include <utility>
#include <variant>
#include <vector>
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/arithm/binaryop.h"
#include "middleend/mlir/dialect/arithm/copy.h"
#include "middleend/mlir/dialect/arithm/unaryop.h"
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

#define GET_MLIR_CONTEXT() compiler_context_->GetIRContext()->GetMLIRContext()

namespace middleend::pass::transform {
namespace {

// Directions of the loop condition comparison, written as (induction variable) OP (bound).
enum class CmpKind : uint8_t { kLT, kLE, kGT, kGE, kNE, kEQ, kInvalid };

// How the induction variable advances each iteration: `arg + operand` or `arg * operand`.
enum class StepKind : uint8_t { kAdd, kMul };
struct InductionStep {
  StepKind kind_{StepKind::kAdd};
  int64_t operand_{0};
};

// Swaps the comparison to keep the induction variable on the left when it was on the right.
auto SwapSides(CmpKind kind) -> CmpKind {
  switch (kind) {
    case CmpKind::kLT:
      return CmpKind::kGT;
    case CmpKind::kLE:
      return CmpKind::kGE;
    case CmpKind::kGT:
      return CmpKind::kLT;
    case CmpKind::kGE:
      return CmpKind::kLE;
    case CmpKind::kNE:
      return CmpKind::kNE;
    case CmpKind::kEQ:
      return CmpKind::kEQ;
    default:
      return CmpKind::kInvalid;
  }
}

auto Evaluate(CmpKind kind, int64_t value, int64_t bound) -> bool {
  switch (kind) {
    case CmpKind::kLT:
      return value < bound;
    case CmpKind::kLE:
      return value <= bound;
    case CmpKind::kGT:
      return value > bound;
    case CmpKind::kGE:
      return value >= bound;
    case CmpKind::kNE:
      return value != bound;
    case CmpKind::kEQ:
      return value == bound;
    default:
      return false;
  }
}

// Advances the simulated induction value by one step, returning false if a multiplicative step
// would overflow int64 (so the trip count cannot be computed safely).
auto AdvanceValue(const InductionStep &step, int64_t *value) -> bool {
  if (step.kind_ == StepKind::kAdd) {
    *value += step.operand_;
    return true;
  }
  auto wide = static_cast<__int128>(*value) * step.operand_;
  if (wide > INT64_MAX || wide < INT64_MIN) {
    return false;
  }
  *value = static_cast<int64_t>(wide);
  return true;
}

// Follows a chain of arithm.copy definitions to the underlying value.
auto TraceCopies(mlir::Value *value) -> mlir::Value * {
  while (value != nullptr) {
    auto *copy = mlir::DynamicOrNull<mlir::arithm::CopyOp>(value->op_);
    if (copy == nullptr) {
      break;
    }
    value = copy->GetValue();
  }
  return value;
}

// If `value` (after copies) is an integer constant, returns {true, constant}.
auto TraceConstInt(mlir::Value *value) -> std::pair<bool, int64_t> {
  value = TraceCopies(value);
  if (value == nullptr) {
    return {false, 0};
  }
  auto *const_op = mlir::DynamicOrNull<mlir::arithm::ConstOp>(value->op_);
  if (const_op == nullptr) {
    return {false, 0};
  }
  auto raw = const_op->GetValue();
  if (!std::holds_alternative<int>(raw)) {
    return {false, 0};
  }
  return {true, static_cast<int64_t>(std::get<int>(raw))};
}

// Returns the index of `value` (after copies) among the block arguments, or -1 if it is not one.
auto IndexOfArgument(mlir::Value *value, mlir::Block *cond_block) -> int {
  value = TraceCopies(value);
  for (size_t i = 0; i < cond_block->arguments_.size(); ++i) {
    if (cond_block->arguments_[i] == value) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

// Reads a comparison operation into (kind, lhs, rhs); kind is kInvalid if it is not a comparison.
auto ReadComparison(mlir::Value *cond_value, CmpKind *kind, mlir::Value **lhs, mlir::Value **rhs) -> void {
  cond_value = TraceCopies(cond_value);
  if (cond_value == nullptr) {
    *kind = CmpKind::kInvalid;
    return;
  }
  auto *op = cond_value->op_;
#define READ_CMP(OP_TYPE, KIND)                                     \
  if (auto *cmp = mlir::DynamicOrNull<mlir::arithm::OP_TYPE>(op)) { \
    *kind = KIND;                                                   \
    *lhs = cmp->GetLHS();                                           \
    *rhs = cmp->GetRHS();                                           \
    return;                                                         \
  }
  READ_CMP(LessOp, CmpKind::kLT)
  READ_CMP(LessEqualOp, CmpKind::kLE)
  READ_CMP(GreaterOp, CmpKind::kGT)
  READ_CMP(GreaterEqualOp, CmpKind::kGE)
  READ_CMP(NotEqualOp, CmpKind::kNE)
  READ_CMP(EqualOp, CmpKind::kEQ)
#undef READ_CMP
  *kind = CmpKind::kInvalid;
}

// If `next` (after copies) is `arg + const` / `arg - const` / `arg * const`, writes the step and
// returns true. The induction variable must be the subtracted-from side of a subtraction.
auto ExtractStep(mlir::Value *next, mlir::Value *induction_arg, InductionStep *step) -> bool {
  next = TraceCopies(next);
  if (next == nullptr) {
    return false;
  }
  if (auto *add = mlir::DynamicOrNull<mlir::arithm::AddOp>(next->op_)) {
    if (TraceCopies(add->GetLHS()) == induction_arg) {
      auto [ok, value] = TraceConstInt(add->GetRHS());
      if (ok) {
        *step = {StepKind::kAdd, value};
        return true;
      }
    }
    if (TraceCopies(add->GetRHS()) == induction_arg) {
      auto [ok, value] = TraceConstInt(add->GetLHS());
      if (ok) {
        *step = {StepKind::kAdd, value};
        return true;
      }
    }
    return false;
  }
  if (auto *sub = mlir::DynamicOrNull<mlir::arithm::SubOp>(next->op_)) {
    if (TraceCopies(sub->GetLHS()) == induction_arg) {
      auto [ok, value] = TraceConstInt(sub->GetRHS());
      if (ok) {
        *step = {StepKind::kAdd, -value};
        return true;
      }
    }
    return false;
  }
  if (auto *mul = mlir::DynamicOrNull<mlir::arithm::MulOp>(next->op_)) {
    if (TraceCopies(mul->GetLHS()) == induction_arg) {
      auto [ok, value] = TraceConstInt(mul->GetRHS());
      if (ok) {
        *step = {StepKind::kMul, value};
        return true;
      }
    }
    if (TraceCopies(mul->GetRHS()) == induction_arg) {
      auto [ok, value] = TraceConstInt(mul->GetLHS());
      if (ok) {
        *step = {StepKind::kMul, value};
        return true;
      }
    }
  }
  return false;
}

}  // namespace

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
  if (!BodyIsUnrollable(while_op->GetBodyRegion())) {
    return false;
  }
  int64_t trip = 0;
  if (!ComputeTripCount(while_op, &trip)) {
    return false;
  }
  auto per_iteration = static_cast<int64_t>(CountOperations(while_op->GetBodyRegion()));
  if (trip * per_iteration > kMaxUnrollCost) {
    return false;
  }
  *trip_count = trip;
  return true;
}

auto MLIRLoopUnrollPass::ComputeTripCount(mlir::scf::WhileOp *while_op, int64_t *trip_count) -> bool {
  auto *cond_region = while_op->GetCondRegion();
  auto *body_region = while_op->GetBodyRegion();
  if (cond_region->blocks_.size() != 1 || body_region->blocks_.size() != 1) {
    return false;
  }
  auto *cond_block = cond_region->blocks_[0];
  auto *body_block = body_region->blocks_[0];
  if (cond_block->operations_.empty() || body_block->operations_.empty()) {
    return false;
  }
  auto *condition_op = mlir::DynamicOrNull<mlir::scf::ConditionOp>(cond_block->operations_.back());
  auto *yield_op = mlir::DynamicOrNull<mlir::scf::YieldOp>(body_block->operations_.back());
  if (condition_op == nullptr || yield_op == nullptr) {
    return false;
  }
  auto original_values = condition_op->GetOriginalValues();
  auto &yield_operands = yield_op->GetOperands();
  size_t size = cond_block->arguments_.size();
  if (original_values.size() != size || yield_operands.size() != size) {
    return false;
  }

  CmpKind kind = CmpKind::kInvalid;
  mlir::Value *lhs = nullptr;
  mlir::Value *rhs = nullptr;
  ReadComparison(condition_op->GetCondition(), &kind, &lhs, &rhs);
  if (kind == CmpKind::kInvalid) {
    return false;
  }
  int induction_index = IndexOfArgument(lhs, cond_block);
  mlir::Value *bound_value = rhs;
  if (induction_index < 0) {
    induction_index = IndexOfArgument(rhs, cond_block);
    bound_value = lhs;
    kind = SwapSides(kind);
    if (induction_index < 0) {
      return false;
    }
  }

  auto [bound_ok, bound] = TraceConstInt(bound_value);
  auto [init_ok, init] = TraceConstInt(original_values[induction_index]);
  if (!bound_ok || !init_ok) {
    return false;
  }
  InductionStep step;
  if (!ExtractStep(yield_operands[induction_index], cond_block->arguments_[induction_index], &step)) {
    return false;
  }

  // Simulate the counted iteration; the cap rejects loops that are too large or (with the wrong
  // step / a never-hit == or != bound) never terminate.
  int64_t value = init;
  int64_t count = 0;
  while (Evaluate(kind, value, bound)) {
    ++count;
    if (count > kMaxTripCount) {
      return false;
    }
    if (!AdvanceValue(step, &value)) {
      return false;
    }
  }
  *trip_count = count;
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

auto MLIRLoopUnrollPass::BodyIsUnrollable(mlir::Region *body) -> bool {
  if (body == nullptr || body->blocks_.size() != 1) {
    return false;
  }
  const auto &operations = body->blocks_[0]->operations_;
  return std::all_of(operations.begin(), operations.end(),
                     [this](mlir::Operation *operation) { return OperationIsUnrollable(operation); });
}

auto MLIRLoopUnrollPass::OperationIsUnrollable(mlir::Operation *operation) -> bool {
  // A nested while cannot be cloned (its condition region holds an scf.condition the cloner does
  // not support); such loops unroll from the inside out on a later round instead.
  if (mlir::DynamicOrNull<mlir::scf::WhileOp>(operation) != nullptr) {
    return false;
  }
  if (mlir::DynamicOrNull<mlir::scf::BreakOp>(operation) != nullptr ||
      mlir::DynamicOrNull<mlir::scf::ContinueOp>(operation) != nullptr ||
      mlir::DynamicOrNull<mlir::scf::ConditionOp>(operation) != nullptr ||
      mlir::DynamicOrNull<mlir::func::ReturnOp>(operation) != nullptr ||
      mlir::DynamicOrNull<mlir::func::FuncDefOp>(operation) != nullptr) {
    return false;
  }
  if (auto *if_op = mlir::DynamicOrNull<mlir::scf::IfOp>(operation)) {
    for (auto *region : {if_op->GetThenRegion(), if_op->GetElseRegion()}) {
      if (region == nullptr || region->blocks_.size() != 1) {
        return false;
      }
      const auto &operations = region->blocks_[0]->operations_;
      if (!std::all_of(operations.begin(), operations.end(),
                       [this](mlir::Operation *inner) { return OperationIsUnrollable(inner); })) {
        return false;
      }
    }
  }
  return true;
}

auto MLIRLoopUnrollPass::CountOperations(mlir::Region *region) -> size_t {
  size_t count = 0;
  for (auto *block : region->blocks_) {
    for (auto *operation : block->operations_) {
      ++count;
      for (auto *nested : operation->regions_) {
        count += CountOperations(nested);
      }
    }
  }
  return count;
}

}  // namespace middleend::pass::transform
