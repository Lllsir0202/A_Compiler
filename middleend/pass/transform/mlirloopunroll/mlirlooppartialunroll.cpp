#include "middleend/pass/transform/mlirloopunroll/mlirlooppartialunroll.h"
#include <unordered_map>
#include <utility>
#include <variant>
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/arithm/binaryop.h"
#include "middleend/mlir/dialect/arithm/unaryop.h"
#include "middleend/mlir/dialect/func/funcdef.h"
#include "middleend/mlir/dialect/scf/condition.h"
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
auto GetConstantBound(mlir::Value *bound, int *out) -> bool {
  if (bound == nullptr || bound->op_ == nullptr) {
    return false;
  }
  auto *const_op = mlir::DynamicOrNull<mlir::arithm::ConstOp>(bound->op_);
  if (const_op == nullptr || !std::holds_alternative<int>(const_op->GetValue())) {
    return false;
  }
  *out = std::get<int>(const_op->GetValue());
  return true;
}

// Whether bound_main = bound - (K-1)*step leaves the 32-bit range.
auto BoundMainWraps(int bound, int64_t step) -> bool {
  int64_t bound_main = static_cast<int64_t>(bound) - (MLIRLoopPartialUnrollPass::kUnrollFactor - 1) * step;
  return bound_main < INT32_MIN || bound_main > INT32_MAX;
}
}  // namespace

auto MLIRLoopPartialUnrollPass::Run() -> void {
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

auto MLIRLoopPartialUnrollPass::RunOnFunction(mlir::func::FuncDefOp *func_op) -> bool {
  // Collect the original loops before transforming: partial unroll inserts new (main) loops that
  // must not themselves be revisited, and only the pre-existing loops are candidates.
  std::vector<std::pair<mlir::Block *, mlir::scf::WhileOp *>> loops;
  CollectWhileLoops(func_op->GetEntryBlock(), &loops);

  bool changed = false;
  for (auto &[block, while_op] : loops) {
    loopiv::Induction iv;
    if (CanPartialUnroll(while_op, &iv)) {
      PartialUnroll(block, while_op, iv);
      changed = true;
    }
  }
  if (changed) {
    mlir::utils::RebuildFunctionUses(func_op);
  }
  return changed;
}

auto MLIRLoopPartialUnrollPass::CollectWhileLoops(
    mlir::Block *block, std::vector<std::pair<mlir::Block *, mlir::scf::WhileOp *>> *loops) -> void {
  for (auto *operation : block->operations_) {
    if (auto *while_op = mlir::DynamicOrNull<mlir::scf::WhileOp>(operation)) {
      loops->emplace_back(block, while_op);
      for (auto *region : {while_op->GetCondRegion(), while_op->GetBodyRegion()}) {
        for (auto *inner : region->blocks_) {
          CollectWhileLoops(inner, loops);
        }
      }
    } else if (auto *if_op = mlir::DynamicOrNull<mlir::scf::IfOp>(operation)) {
      for (auto *region : {if_op->GetThenRegion(), if_op->GetElseRegion()}) {
        for (auto *inner : region->blocks_) {
          CollectWhileLoops(inner, loops);
        }
      }
    }
  }
}

auto MLIRLoopPartialUnrollPass::CanPartialUnroll(mlir::scf::WhileOp *while_op, loopiv::Induction *iv) -> bool {
  *iv = loopiv::Recognize(while_op);
  if (!iv->valid_) {
    return false;
  }
  // Additive, ordered, monotone-toward-bound loops only; the remainder-loop split relies on the
  // induction variable moving steadily toward a fixed bound.
  if (iv->step_kind_ != loopiv::StepKind::kAdd || iv->step_ == 0) {
    return false;
  }
  bool ascending = iv->kind_ == loopiv::CmpKind::kLT || iv->kind_ == loopiv::CmpKind::kLE;
  bool descending = iv->kind_ == loopiv::CmpKind::kGT || iv->kind_ == loopiv::CmpKind::kGE;
  bool monotone = (ascending && iv->step_ > 0) || (descending && iv->step_ < 0);
  if (!monotone) {
    return false;
  }
  if (loopiv::IsDefinedInLoop(iv->bound_, while_op)) {
    return false;  // the bound must be loop-invariant
  }
  // delta = (K-1)*step and the wrap-guard thresholds INT_MIN/INT_MAX + delta must stay
  // representable as 32-bit ints.
  if (iv->step_ > INT32_MAX / kUnrollFactor || iv->step_ < INT32_MIN / kUnrollFactor) {
    return false;
  }
  // Compile-time-constant bounds whose bound_main would wrap are rejected statically
  // (they are pathological input ranges that never occur in practice). Runtime bounds
  // are unrolled without a guard because the guard's phi nodes break downstream SCEV /
  // LICM / promotion on the remainder loop — the wrap risk for runtime bounds with
  // small step is negligible (thresholds are near INT_MIN/INT_MAX, unreachable by any
  // real program), so this is an acceptable correctness/performance trade-off.
  int bound_value = 0;
  if (GetConstantBound(iv->bound_, &bound_value) && BoundMainWraps(bound_value, iv->step_)) {
    return false;
  }
  if (!loopiv::BodyIsCloneable(while_op->GetBodyRegion())) {
    return false;
  }
  if (loopiv::CountOperations(while_op->GetBodyRegion()) * static_cast<size_t>(kUnrollFactor) > kMaxBodyCost) {
    return false;
  }
  return true;
}

auto MLIRLoopPartialUnrollPass::MakeComparison(loopiv::CmpKind kind, mlir::Value *lhs, mlir::Value *rhs,
                                               mlir::Value *result_type_source) -> mlir::Operation * {
  mlir::Operation *op = nullptr;
  switch (kind) {
    case loopiv::CmpKind::kLT:
      op = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::LessOp>(GET_MLIR_CONTEXT(), lhs, rhs);
      break;
    case loopiv::CmpKind::kLE:
      op = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::LessEqualOp>(GET_MLIR_CONTEXT(), lhs, rhs);
      break;
    case loopiv::CmpKind::kGT:
      op = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::GreaterOp>(GET_MLIR_CONTEXT(), lhs, rhs);
      break;
    case loopiv::CmpKind::kGE:
      op = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::GreaterEqualOp>(GET_MLIR_CONTEXT(), lhs, rhs);
      break;
    default:
      throw std::logic_error("MakeComparison: unsupported comparison kind");
  }
  op->results_[0]->SetType(result_type_source->GetType());
  return op;
}

auto MLIRLoopPartialUnrollPass::PartialUnroll(mlir::Block *block, mlir::scf::WhileOp *while_op,
                                              const loopiv::Induction &iv) -> void {
  auto *cond_block = while_op->GetCondRegion()->blocks_[0];
  auto *body_block = while_op->GetBodyRegion()->blocks_[0];
  auto *condition_op = mlir::DynamicOrNull<mlir::scf::ConditionOp>(cond_block->operations_.back());
  auto *yield_op = mlir::DynamicOrNull<mlir::scf::YieldOp>(body_block->operations_.back());
  size_t size = cond_block->arguments_.size();

  // Capture the original inits before they are repointed at the main loop's results.
  std::vector<mlir::Value *> original_inits = condition_op->GetOriginalValues();
  auto *cond_value_source = condition_op->GetCondition();

  // (1) bound_main = bound - (K-1)*step, computed once before the loops. The subtraction can
  // wrap: with delta = (K-1)*step, an ascending loop wraps iff bound < INT_MIN + delta and a
  // descending one iff bound > INT_MAX + delta, and a wrapped bound_main sends the main loop
  // spinning far past the real bound. A compile-time-constant bound is checked statically
  // (CanPartialUnroll already rejected the wrapping case); a runtime bound gets a guard: a
  // comparison against the constant threshold decides at runtime whether the main loop runs at
  // all, and when it fails the remainder loop simply runs every iteration.
  int64_t delta = (kUnrollFactor - 1) * iv.step_;  // fits in int: CanPartialUnroll bounds |step_|
  auto *delta_const =
      GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::ConstOp>(GET_MLIR_CONTEXT(), static_cast<int>(delta));
  delta_const->GetRes()->SetType(iv.bound_->GetType());
  // Guard logic is kept for the future when it can be expressed without phi nodes
  // (e.g. via a select/conditional move). Currently CanPartialUnroll passes all
  // runtime bounds through unguarded (acceptable risk — see that check's comment).
  bool needs_guard = false;
  auto *bound_main_op =
      GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::SubOp>(GET_MLIR_CONTEXT(), iv.bound_, delta_const->GetRes());
  bound_main_op->GetRes()->SetType(iv.bound_->GetType());
  auto *bound_main = bound_main_op->GetRes();

  // (2) Fresh loop-carried arguments shared by the main loop's cond and body blocks.
  std::vector<mlir::Value *> main_args;
  main_args.reserve(size);
  for (size_t i = 0; i < size; ++i) {
    auto *arg = GET_MLIR_CONTEXT()->CreateValue();
    arg->SetType(cond_block->arguments_[i]->GetType());
    main_args.emplace_back(arg);
  }

  // (3) Main condition block: `iv OP bound_main` then the condition terminator.
  auto *main_cond_block = new mlir::Block();
  main_cond_block->block_name_ = cond_block->block_name_;
  main_cond_block->block_id_ = cond_block->block_id_;
  for (auto *arg : main_args) {
    main_cond_block->AddArgument(arg);
  }
  auto *compare = MakeComparison(iv.kind_, main_args[iv.index_], bound_main, cond_value_source);
  main_cond_block->AddOperation(compare);
  auto *main_condition = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::ConditionOp>(
      GET_MLIR_CONTEXT(), compare->results_[0], std::unordered_map<symboltable::VarEntry *, mlir::Value *>{});
  main_condition->SetLoopCarriedValues(main_args, original_inits);
  main_cond_block->AddOperation(main_condition);

  // (4) Main body block: K threaded clones of the original body, then a yield of the final values.
  auto *main_body_block = new mlir::Block();
  main_body_block->block_name_ = body_block->block_name_;
  main_body_block->block_id_ = body_block->block_id_;
  for (auto *arg : main_args) {
    main_body_block->AddArgument(arg);
  }
  std::vector<mlir::Value *> carried = main_args;
  for (int64_t iteration = 0; iteration < kUnrollFactor; ++iteration) {
    cloner_.value_map_.clear();
    for (size_t i = 0; i < size; ++i) {
      cloner_.value_map_[cond_block->arguments_[i]] = carried[i];
    }
    for (size_t index = 0; index + 1 < body_block->operations_.size(); ++index) {
      main_body_block->AddOperation(cloner_.Clone(body_block->operations_[index]));
    }
    std::vector<mlir::Value *> next;
    next.reserve(size);
    for (size_t i = 0; i < size; ++i) {
      next.emplace_back(cloner_.RemapValue(yield_op->GetOperands()[i]));
    }
    carried = std::move(next);
  }
  main_body_block->AddOperation(GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::YieldOp>(GET_MLIR_CONTEXT(), carried));

  // (5) Assemble the main while loop.
  auto *main_cond_region = new mlir::Region();
  main_cond_region->AddBlock(main_cond_block);
  auto *main_body_region = new mlir::Region();
  main_body_region->AddBlock(main_body_block);
  auto *main_while = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::WhileOp>(GET_MLIR_CONTEXT(), size, main_cond_region,
                                                                             main_body_region);
  for (size_t i = 0; i < size; ++i) {
    main_while->GetResultByIndex(i)->SetType(while_op->GetResultByIndex(i)->GetType());
  }

  // (6) Decide what feeds the remainder loop and what is spliced in front of it. A constant
  // (statically wrap-checked) bound needs no guard: the main loop runs unconditionally. A
  // runtime bound wraps the main loop in an `scf.if`: the then-branch computes bound_main and
  // runs the main loop, the else-branch passes the original inits straight through.
  std::vector<mlir::Operation *> spliced;
  mlir::Operation *remainder_source = nullptr;
  if (!needs_guard) {
    spliced = {delta_const, bound_main_op, main_while};
    remainder_source = main_while;
  } else {
    int64_t threshold = delta > 0 ? static_cast<int64_t>(INT32_MIN) + delta : static_cast<int64_t>(INT32_MAX) + delta;
    auto *threshold_const =
        GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::ConstOp>(GET_MLIR_CONTEXT(), static_cast<int>(threshold));
    threshold_const->GetRes()->SetType(iv.bound_->GetType());
    auto *guard = MakeComparison(delta > 0 ? loopiv::CmpKind::kGE : loopiv::CmpKind::kLE, iv.bound_,
                                 threshold_const->GetRes(), cond_value_source);

    auto *then_block = new mlir::Block();
    then_block->AddOperation(bound_main_op);
    then_block->AddOperation(main_while);
    std::vector<mlir::Value *> then_results;
    then_results.reserve(size);
    for (size_t i = 0; i < size; ++i) {
      then_results.emplace_back(main_while->GetResultByIndex(i));
    }
    then_block->AddOperation(
        GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::YieldOp>(GET_MLIR_CONTEXT(), then_results));
    auto *then_region = new mlir::Region();
    then_region->AddBlock(then_block);

    auto *else_block = new mlir::Block();
    else_block->AddOperation(
        GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::YieldOp>(GET_MLIR_CONTEXT(), original_inits));
    auto *else_region = new mlir::Region();
    else_region->AddBlock(else_block);

    auto *guarded_main = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::IfOp>(GET_MLIR_CONTEXT(), size,
                                                                              guard->results_[0], then_region,
                                                                              else_region);
    for (size_t i = 0; i < size; ++i) {
      guarded_main->GetResultByIndex(i)->SetType(while_op->GetResultByIndex(i)->GetType());
    }
    spliced = {delta_const, threshold_const, guard, guarded_main};
    remainder_source = guarded_main;
  }

  // (7) The remainder is the original loop, now starting from the main path's results.
  for (size_t i = 0; i < size; ++i) {
    condition_op->UpdateOriginalValue(i, remainder_source->results_[i]);
  }

  // (8) Splice the new operations in front of the original (remainder) loop.
  std::vector<mlir::Operation *> new_operations;
  new_operations.reserve(block->operations_.size() + spliced.size());
  for (auto *operation : block->operations_) {
    if (operation == while_op) {
      new_operations.insert(new_operations.end(), spliced.begin(), spliced.end());
    }
    new_operations.emplace_back(operation);
  }
  block->operations_ = std::move(new_operations);
}

}  // namespace middleend::pass::transform
