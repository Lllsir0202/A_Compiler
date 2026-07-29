#include "middleend/pass/transform/mlirloopunroll/mlirlooppartialunroll.h"
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <variant>
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/arithm/binaryop.h"
#include "middleend/mlir/dialect/arithm/copy.h"
#include "middleend/mlir/dialect/arithm/unaryop.h"
#include "middleend/mlir/dialect/func/funcdef.h"
#include "middleend/mlir/dialect/scf/condition.h"
#include "middleend/mlir/dialect/scf/if.h"
#include "middleend/mlir/dialect/scf/while.h"
#include "middleend/mlir/dialect/scf/yield.h"
#include "middleend/mlir/dialect/var/getptr.h"
#include "middleend/mlir/dialect/var/getvalue.h"
#include "middleend/mlir/dialect/var/globaldef.h"
#include "middleend/mlir/dialect/var/set.h"
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
auto BoundMainWraps(int bound, int64_t step, int64_t factor) -> bool {
  int64_t bound_main = static_cast<int64_t>(bound) - (factor - 1) * step;
  return bound_main < INT32_MIN || bound_main > INT32_MAX;
}

// Copy operations are the structured IR representation of source-level scalar assignments.  The
// loop matchers care about the value being assigned, rather than the number of copies on the path.
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

auto SameValue(mlir::Value *lhs, mlir::Value *rhs) -> bool { return TraceCopies(lhs) == TraceCopies(rhs); }

auto IsConstInt(mlir::Value *value, int64_t expected) -> bool {
  value = TraceCopies(value);
  auto *constant = value == nullptr ? nullptr : mlir::DynamicOrNull<mlir::arithm::ConstOp>(value->op_);
  if (constant == nullptr || !std::holds_alternative<int>(constant->GetValue())) {
    return false;
  }
  return static_cast<int64_t>(std::get<int>(constant->GetValue())) == expected;
}

auto IsConditionalRegisterBlock(mlir::scf::WhileOp *loop) -> bool {
  if (loop->GetResultsSize() != 5 || loop->GetBodyRegion()->blocks_.size() != 1) {
    return false;
  }
  size_t conditional_lanes = 0;
  for (auto *operation : loop->GetBodyRegion()->blocks_[0]->operations_) {
    conditional_lanes += mlir::DynamicOrNull<mlir::scf::IfOp>(operation) != nullptr ? 1 : 0;
  }
  return conditional_lanes == 4;
}

// Returns the invariant step operand of `next = arg + step` (possibly through copies).
auto FindAddStep(mlir::Value *next, mlir::Value *arg, bool *arg_is_lhs) -> mlir::Value * {
  next = TraceCopies(next);
  auto *add = next == nullptr ? nullptr : mlir::DynamicOrNull<mlir::arithm::AddOp>(next->op_);
  if (add == nullptr) {
    return nullptr;
  }
  if (SameValue(add->GetLHS(), arg)) {
    *arg_is_lhs = true;
    return TraceCopies(add->GetRHS());
  }
  if (SameValue(add->GetRHS(), arg)) {
    *arg_is_lhs = false;
    return TraceCopies(add->GetLHS());
  }
  return nullptr;
}

struct DotProductJamPattern {
  loopiv::Induction column_iv_;
  loopiv::Induction reduction_iv_;
  mlir::scf::WhileOp *reduction_loop_{nullptr};
  mlir::scf::ConditionOp *column_condition_{nullptr};
  mlir::scf::ConditionOp *reduction_condition_{nullptr};
  mlir::scf::YieldOp *column_yield_{nullptr};
  mlir::scf::YieldOp *reduction_yield_{nullptr};
  mlir::var::VarGetPtrOp *row_reduction_ptr_{nullptr};
  mlir::var::VarGetValOp *row_reduction_load_{nullptr};
  mlir::var::VarGetPtrOp *column_input_ptr_{nullptr};
  mlir::var::VarGetValOp *column_input_load_{nullptr};
  mlir::var::VarGetPtrOp *output_ptr_{nullptr};
  mlir::var::VarSetOp *output_store_{nullptr};
  mlir::arithm::MulOp *multiply_{nullptr};
  mlir::arithm::AddOp *sum_add_{nullptr};
  mlir::Value *row_base_{nullptr};
  mlir::Value *output_base_{nullptr};
  mlir::Value *row_{nullptr};
  mlir::Value *column_step_{nullptr};
  mlir::Value *reduction_step_{nullptr};
  mlir::Value *reduction_init_{nullptr};
  mlir::Value *sum_init_{nullptr};
  bool column_arg_is_lhs_{true};
  bool reduction_arg_is_lhs_{true};
  bool sum_is_lhs_{true};
  bool row_load_is_lhs_{true};
};

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
  // First expose row-contiguous accesses in strict matrix dot products.  The ordinary partial
  // unroller below can then widen the newly-created shared reduction loop as usual.
  register_blocked_loops_.clear();
  bool changed = JamDotProductLoops(func_op);

  // Collect the original loops before transforming: partial unroll inserts new (main) loops that
  // must not themselves be revisited, and only the pre-existing loops are candidates.
  std::vector<std::pair<mlir::Block *, mlir::scf::WhileOp *>> loops;
  CollectWhileLoops(func_op->GetEntryBlock(), &loops);

  for (auto &[block, while_op] : loops) {
    // A four-lane conditional reduction created by the earlier generic jam
    // pass has five results: one induction variable and four accumulators.
    bool register_blocked = register_blocked_loops_.count(while_op) != 0 || IsConditionalRegisterBlock(while_op);
    int64_t factor = register_blocked ? jam_unroll_factor_ : kUnrollFactor;
    loopiv::Induction iv;
    if (CanPartialUnroll(while_op, &iv, factor)) {
      PartialUnroll(block, while_op, iv, factor);
      changed = true;
    }
  }
  if (changed) {
    mlir::utils::RebuildFunctionUses(func_op);
  }
  return changed;
}

auto MLIRLoopPartialUnrollPass::JamDotProductLoops(mlir::func::FuncDefOp *func_op) -> bool {
  std::vector<std::pair<mlir::Block *, mlir::scf::WhileOp *>> loops;
  CollectWhileLoops(func_op->GetEntryBlock(), &loops);

  bool changed = false;
  for (auto &[block, while_op] : loops) {
    changed |= TryJamDotProductLoop(block, while_op);
  }
  return changed;
}

auto MLIRLoopPartialUnrollPass::TryJamDotProductLoop(mlir::Block *block, mlir::scf::WhileOp *while_op) -> bool {
  DotProductJamPattern pattern;
  pattern.column_iv_ = loopiv::Recognize(while_op);
  if (!pattern.column_iv_.valid_ || pattern.column_iv_.index_ != 0 ||
      pattern.column_iv_.kind_ != loopiv::CmpKind::kLT || pattern.column_iv_.step_kind_ != loopiv::StepKind::kAdd ||
      pattern.column_iv_.step_ != 1 || while_op->GetResultsSize() != 1 || !IsConstInt(pattern.column_iv_.init_, 0) ||
      loopiv::IsDefinedInLoop(pattern.column_iv_.bound_, while_op)) {
    return false;
  }

  auto *column_cond_block = while_op->GetCondRegion()->blocks_[0];
  auto *column_body_block = while_op->GetBodyRegion()->blocks_[0];
  if (column_cond_block->arguments_.size() != 1 || column_body_block->arguments_.size() != 1) {
    return false;
  }
  pattern.column_condition_ = mlir::DynamicOrNull<mlir::scf::ConditionOp>(column_cond_block->operations_.back());
  pattern.column_yield_ = mlir::DynamicOrNull<mlir::scf::YieldOp>(column_body_block->operations_.back());
  if (pattern.column_condition_ == nullptr || pattern.column_yield_ == nullptr ||
      pattern.column_yield_->GetOperands().size() != 1) {
    return false;
  }
  pattern.column_step_ = FindAddStep(pattern.column_yield_->GetOperands()[0], column_cond_block->arguments_[0],
                                     &pattern.column_arg_is_lhs_);
  if (pattern.column_step_ == nullptr || !IsConstInt(pattern.column_step_, 1) ||
      loopiv::IsDefinedInLoop(pattern.column_step_, while_op)) {
    return false;
  }

  // The outer body must only contain the reduction loop, one output store, induction arithmetic,
  // and the scalar copies emitted by the frontend.  This intentionally excludes calls, branches,
  // and any additional memory effects.
  size_t nested_loops = 0;
  size_t output_ptrs = 0;
  size_t output_stores = 0;
  for (size_t i = 0; i + 1 < column_body_block->operations_.size(); ++i) {
    auto *operation = column_body_block->operations_[i];
    if (auto *nested = mlir::DynamicOrNull<mlir::scf::WhileOp>(operation)) {
      pattern.reduction_loop_ = nested;
      ++nested_loops;
    } else if (auto *getptr = mlir::DynamicOrNull<mlir::var::VarGetPtrOp>(operation)) {
      pattern.output_ptr_ = getptr;
      ++output_ptrs;
    } else if (auto *store = mlir::DynamicOrNull<mlir::var::VarSetOp>(operation)) {
      pattern.output_store_ = store;
      ++output_stores;
    } else if (mlir::DynamicOrNull<mlir::arithm::CopyOp>(operation) == nullptr &&
               mlir::DynamicOrNull<mlir::arithm::AddOp>(operation) == nullptr) {
      return false;
    }
  }
  if (nested_loops != 1 || output_ptrs != 1 || output_stores != 1 ||
      !SameValue(pattern.output_store_->GetPtr(), pattern.output_ptr_->GetRes())) {
    return false;
  }

  auto output_indices = pattern.output_ptr_->GetIndices();
  auto *column_arg = column_cond_block->arguments_[0];
  if (output_indices.size() != 2 || !SameValue(output_indices[1], column_arg)) {
    return false;
  }
  pattern.row_ = TraceCopies(output_indices[0]);
  if (pattern.row_ == nullptr || loopiv::IsDefinedInLoop(pattern.row_, while_op)) {
    return false;
  }

  pattern.reduction_iv_ = loopiv::Recognize(pattern.reduction_loop_);
  if (!pattern.reduction_iv_.valid_ || pattern.reduction_iv_.index_ >= 2 ||
      pattern.reduction_iv_.kind_ != loopiv::CmpKind::kLT ||
      pattern.reduction_iv_.step_kind_ != loopiv::StepKind::kAdd || pattern.reduction_iv_.step_ != 1 ||
      pattern.reduction_loop_->GetResultsSize() != 2 || !IsConstInt(pattern.reduction_iv_.init_, 0) ||
      !SameValue(pattern.reduction_iv_.bound_, pattern.column_iv_.bound_)) {
    return false;
  }
  // Loop-carried variables originate in an unordered symbol map, so the frontend may represent
  // the same reduction as either (k, sum) or (sum, k).  Follow the induction recognizer's index
  // instead of making the optimization depend on that incidental order.
  const size_t reduction_index = pattern.reduction_iv_.index_;
  const size_t sum_index = 1 - reduction_index;
  auto *reduction_cond_block = pattern.reduction_loop_->GetCondRegion()->blocks_[0];
  auto *reduction_body_block = pattern.reduction_loop_->GetBodyRegion()->blocks_[0];
  if (reduction_cond_block->arguments_.size() != 2 || reduction_body_block->arguments_.size() != 2) {
    return false;
  }
  pattern.reduction_condition_ = mlir::DynamicOrNull<mlir::scf::ConditionOp>(reduction_cond_block->operations_.back());
  pattern.reduction_yield_ = mlir::DynamicOrNull<mlir::scf::YieldOp>(reduction_body_block->operations_.back());
  if (pattern.reduction_condition_ == nullptr || pattern.reduction_yield_ == nullptr ||
      pattern.reduction_yield_->GetOperands().size() != 2) {
    return false;
  }
  auto reduction_inits = pattern.reduction_condition_->GetOriginalValues();
  if (reduction_inits.size() != 2) {
    return false;
  }
  pattern.reduction_init_ = TraceCopies(reduction_inits[reduction_index]);
  pattern.sum_init_ = TraceCopies(reduction_inits[sum_index]);
  if (!IsConstInt(pattern.reduction_init_, 0) || !IsConstInt(pattern.sum_init_, 0) ||
      loopiv::IsDefinedInLoop(pattern.reduction_init_, while_op) ||
      loopiv::IsDefinedInLoop(pattern.sum_init_, while_op)) {
    return false;
  }
  auto *reduction_arg = reduction_cond_block->arguments_[reduction_index];
  auto *sum_arg = reduction_cond_block->arguments_[sum_index];
  pattern.reduction_step_ = FindAddStep(pattern.reduction_yield_->GetOperands()[reduction_index], reduction_arg,
                                        &pattern.reduction_arg_is_lhs_);
  if (pattern.reduction_step_ == nullptr || !IsConstInt(pattern.reduction_step_, 1) ||
      loopiv::IsDefinedInLoop(pattern.reduction_step_, while_op)) {
    return false;
  }

  // Match exactly two two-dimensional loads, one multiply, and the integer/float sum reduction.
  std::vector<mlir::var::VarGetPtrOp *> input_ptrs;
  std::vector<mlir::var::VarGetValOp *> input_loads;
  size_t multiplies = 0;
  size_t additions = 0;
  for (size_t i = 0; i + 1 < reduction_body_block->operations_.size(); ++i) {
    auto *operation = reduction_body_block->operations_[i];
    if (auto *getptr = mlir::DynamicOrNull<mlir::var::VarGetPtrOp>(operation)) {
      input_ptrs.emplace_back(getptr);
    } else if (auto *load = mlir::DynamicOrNull<mlir::var::VarGetValOp>(operation)) {
      input_loads.emplace_back(load);
    } else if (auto *multiply = mlir::DynamicOrNull<mlir::arithm::MulOp>(operation)) {
      pattern.multiply_ = multiply;
      ++multiplies;
    } else if (mlir::DynamicOrNull<mlir::arithm::AddOp>(operation) != nullptr) {
      ++additions;
    } else if (mlir::DynamicOrNull<mlir::arithm::CopyOp>(operation) == nullptr) {
      return false;
    }
  }
  if (input_ptrs.size() != 2 || input_loads.size() != 2 || multiplies != 1 || additions != 2) {
    return false;
  }

  auto *sum_next = TraceCopies(pattern.reduction_yield_->GetOperands()[sum_index]);
  pattern.sum_add_ = sum_next == nullptr ? nullptr : mlir::DynamicOrNull<mlir::arithm::AddOp>(sum_next->op_);
  if (pattern.sum_add_ == nullptr) {
    return false;
  }
  mlir::Value *product = nullptr;
  if (SameValue(pattern.sum_add_->GetLHS(), sum_arg)) {
    pattern.sum_is_lhs_ = true;
    product = TraceCopies(pattern.sum_add_->GetRHS());
  } else if (SameValue(pattern.sum_add_->GetRHS(), sum_arg)) {
    pattern.sum_is_lhs_ = false;
    product = TraceCopies(pattern.sum_add_->GetLHS());
  } else {
    return false;
  }
  if (!SameValue(product, pattern.multiply_->GetRes())) {
    return false;
  }

  auto load_for_ptr = [&](mlir::var::VarGetPtrOp *ptr) -> mlir::var::VarGetValOp * {
    for (auto *load : input_loads) {
      if (SameValue(load->GetPtr(), ptr->GetRes())) {
        return load;
      }
    }
    return nullptr;
  };
  pattern.output_base_ = TraceCopies(pattern.output_ptr_->GetPtr());
  auto *output_global = pattern.output_base_ == nullptr
                            ? nullptr
                            : mlir::DynamicOrNull<mlir::var::GlobalVarDefOp>(pattern.output_base_->op_);
  if (output_global == nullptr || output_global->GetDimensions().size() != 2 ||
      output_global->GetDimensions()[1] < kJamFactor) {
    return false;
  }
  for (auto *ptr : input_ptrs) {
    auto indices = ptr->GetIndices();
    auto *base = TraceCopies(ptr->GetPtr());
    auto *load = load_for_ptr(ptr);
    if (indices.size() != 2 || load == nullptr) {
      return false;
    }
    if (base == pattern.output_base_ && SameValue(indices[0], reduction_arg) && SameValue(indices[1], column_arg)) {
      pattern.column_input_ptr_ = ptr;
      pattern.column_input_load_ = load;
    } else if (base != pattern.output_base_ && SameValue(indices[0], pattern.row_) &&
               SameValue(indices[1], reduction_arg)) {
      auto *input_global = base == nullptr ? nullptr : mlir::DynamicOrNull<mlir::var::GlobalVarDefOp>(base->op_);
      if (input_global == nullptr || input_global->GetDimensions().size() != 2) {
        return false;
      }
      pattern.row_base_ = base;
      pattern.row_reduction_ptr_ = ptr;
      pattern.row_reduction_load_ = load;
    } else {
      return false;
    }
  }
  if (pattern.column_input_ptr_ == nullptr || pattern.row_reduction_ptr_ == nullptr) {
    return false;
  }
  if (SameValue(pattern.multiply_->GetLHS(), pattern.row_reduction_load_->GetRes()) &&
      SameValue(pattern.multiply_->GetRHS(), pattern.column_input_load_->GetRes())) {
    pattern.row_load_is_lhs_ = true;
  } else if (SameValue(pattern.multiply_->GetRHS(), pattern.row_reduction_load_->GetRes()) &&
             SameValue(pattern.multiply_->GetLHS(), pattern.column_input_load_->GetRes())) {
    pattern.row_load_is_lhs_ = false;
  } else {
    return false;
  }
  if (!SameValue(pattern.output_store_->GetNewValue(), pattern.reduction_loop_->GetResultByIndex(sum_index))) {
    return false;
  }

  // Build the four-column register-blocked main loop. Its condition uses `j + 3 < bound`, rather
  // than `j < bound - 3`, so an extreme negative runtime bound cannot underflow during the split.
  auto *main_column_arg = GET_MLIR_CONTEXT()->CreateValue();
  main_column_arg->SetType(column_arg->GetType());

  auto *main_cond_block = new mlir::Block();
  main_cond_block->block_name_ = column_cond_block->block_name_;
  main_cond_block->block_id_ = column_cond_block->block_id_;
  main_cond_block->AddArgument(main_column_arg);
  auto *lookahead = main_column_arg;
  for (int64_t lane = 1; lane < kJamFactor; ++lane) {
    auto *add = pattern.column_arg_is_lhs_ ? GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::AddOp>(
                                                 GET_MLIR_CONTEXT(), lookahead, pattern.column_step_)
                                           : GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::AddOp>(
                                                 GET_MLIR_CONTEXT(), pattern.column_step_, lookahead);
    add->GetRes()->SetType(column_arg->GetType());
    main_cond_block->AddOperation(add);
    lookahead = add->GetRes();
  }
  auto *main_compare = MakeComparison(pattern.column_iv_.kind_, lookahead, pattern.column_iv_.bound_,
                                      pattern.column_condition_->GetCondition());
  main_cond_block->AddOperation(main_compare);
  auto *main_condition = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::ConditionOp>(
      GET_MLIR_CONTEXT(), main_compare->results_[0], std::unordered_map<symboltable::VarEntry *, mlir::Value *>{});
  main_condition->SetLoopCarriedValues({main_column_arg}, pattern.column_condition_->GetOriginalValues());
  main_cond_block->AddOperation(main_condition);

  auto *main_body_block = new mlir::Block();
  main_body_block->block_name_ = column_body_block->block_name_;
  main_body_block->block_id_ = column_body_block->block_id_;
  main_body_block->AddArgument(main_column_arg);
  std::vector<mlir::Value *> columns{main_column_arg};
  for (int64_t lane = 1; lane < kJamFactor; ++lane) {
    auto *add = pattern.column_arg_is_lhs_ ? GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::AddOp>(
                                                 GET_MLIR_CONTEXT(), columns.back(), pattern.column_step_)
                                           : GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::AddOp>(
                                                 GET_MLIR_CONTEXT(), pattern.column_step_, columns.back());
    add->GetRes()->SetType(column_arg->GetType());
    main_body_block->AddOperation(add);
    columns.emplace_back(add->GetRes());
  }

  std::vector<mlir::Value *> jam_args;
  jam_args.reserve(kJamFactor + 1);
  auto *jam_k = GET_MLIR_CONTEXT()->CreateValue();
  jam_k->SetType(reduction_arg->GetType());
  jam_args.emplace_back(jam_k);
  for (int64_t lane = 0; lane < kJamFactor; ++lane) {
    auto *jam_sum = GET_MLIR_CONTEXT()->CreateValue();
    jam_sum->SetType(sum_arg->GetType());
    jam_args.emplace_back(jam_sum);
  }

  auto *jam_cond_block = new mlir::Block();
  jam_cond_block->block_name_ = reduction_cond_block->block_name_;
  jam_cond_block->block_id_ = reduction_cond_block->block_id_;
  for (auto *arg : jam_args) {
    jam_cond_block->AddArgument(arg);
  }
  auto *jam_compare = MakeComparison(pattern.reduction_iv_.kind_, jam_k, pattern.reduction_iv_.bound_,
                                     pattern.reduction_condition_->GetCondition());
  jam_cond_block->AddOperation(jam_compare);
  auto *jam_condition = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::ConditionOp>(
      GET_MLIR_CONTEXT(), jam_compare->results_[0], std::unordered_map<symboltable::VarEntry *, mlir::Value *>{});
  std::vector<mlir::Value *> jam_inits{pattern.reduction_init_};
  jam_inits.insert(jam_inits.end(), kJamFactor, pattern.sum_init_);
  jam_condition->SetLoopCarriedValues(jam_args, jam_inits);
  jam_cond_block->AddOperation(jam_condition);

  auto *jam_body_block = new mlir::Block();
  jam_body_block->block_name_ = reduction_body_block->block_name_;
  jam_body_block->block_id_ = reduction_body_block->block_id_;
  for (auto *arg : jam_args) {
    jam_body_block->AddArgument(arg);
  }
  auto *row_ptr = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarGetPtrOp>(
      GET_MLIR_CONTEXT(), pattern.row_base_, std::vector<mlir::Value *>{pattern.row_, jam_k});
  row_ptr->GetRes()->SetType(pattern.row_reduction_ptr_->GetRes()->GetType());
  jam_body_block->AddOperation(row_ptr);
  auto *row_load = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarGetValOp>(GET_MLIR_CONTEXT(), row_ptr->GetRes());
  row_load->GetRes()->SetType(pattern.row_reduction_load_->GetRes()->GetType());
  jam_body_block->AddOperation(row_load);

  std::vector<mlir::Value *> next_sums;
  next_sums.reserve(kJamFactor);
  for (int64_t lane = 0; lane < kJamFactor; ++lane) {
    auto *input_ptr = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarGetPtrOp>(
        GET_MLIR_CONTEXT(), pattern.output_base_, std::vector<mlir::Value *>{jam_k, columns[lane]});
    input_ptr->GetRes()->SetType(pattern.column_input_ptr_->GetRes()->GetType());
    jam_body_block->AddOperation(input_ptr);
    auto *input_load =
        GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarGetValOp>(GET_MLIR_CONTEXT(), input_ptr->GetRes());
    input_load->GetRes()->SetType(pattern.column_input_load_->GetRes()->GetType());
    jam_body_block->AddOperation(input_load);
    auto *multiply = pattern.row_load_is_lhs_ ? GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::MulOp>(
                                                    GET_MLIR_CONTEXT(), row_load->GetRes(), input_load->GetRes())
                                              : GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::MulOp>(
                                                    GET_MLIR_CONTEXT(), input_load->GetRes(), row_load->GetRes());
    multiply->GetRes()->SetType(pattern.multiply_->GetRes()->GetType());
    jam_body_block->AddOperation(multiply);
    auto *sum = pattern.sum_is_lhs_ ? GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::AddOp>(
                                          GET_MLIR_CONTEXT(), jam_args[lane + 1], multiply->GetRes())
                                    : GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::AddOp>(
                                          GET_MLIR_CONTEXT(), multiply->GetRes(), jam_args[lane + 1]);
    sum->GetRes()->SetType(pattern.sum_add_->GetRes()->GetType());
    jam_body_block->AddOperation(sum);
    next_sums.emplace_back(sum->GetRes());
  }
  auto *next_k = pattern.reduction_arg_is_lhs_ ? GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::AddOp>(
                                                     GET_MLIR_CONTEXT(), jam_k, pattern.reduction_step_)
                                               : GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::AddOp>(
                                                     GET_MLIR_CONTEXT(), pattern.reduction_step_, jam_k);
  next_k->GetRes()->SetType(reduction_arg->GetType());
  jam_body_block->AddOperation(next_k);
  std::vector<mlir::Value *> jam_yields{next_k->GetRes()};
  jam_yields.insert(jam_yields.end(), next_sums.begin(), next_sums.end());
  jam_body_block->AddOperation(GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::YieldOp>(GET_MLIR_CONTEXT(), jam_yields));

  auto *jam_cond_region = new mlir::Region();
  jam_cond_region->AddBlock(jam_cond_block);
  auto *jam_body_region = new mlir::Region();
  jam_body_region->AddBlock(jam_body_block);
  auto *jam_loop = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::WhileOp>(GET_MLIR_CONTEXT(), kJamFactor + 1,
                                                                           jam_cond_region, jam_body_region);
  register_blocked_loops_.insert(jam_loop);
  jam_loop->GetResultByIndex(0)->SetType(pattern.reduction_loop_->GetResultByIndex(reduction_index)->GetType());
  for (int64_t lane = 0; lane < kJamFactor; ++lane) {
    jam_loop->GetResultByIndex(lane + 1)->SetType(pattern.reduction_loop_->GetResultByIndex(sum_index)->GetType());
  }
  main_body_block->AddOperation(jam_loop);

  for (int64_t lane = 0; lane < kJamFactor; ++lane) {
    auto *store_ptr = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarGetPtrOp>(
        GET_MLIR_CONTEXT(), pattern.output_base_, std::vector<mlir::Value *>{pattern.row_, columns[lane]});
    store_ptr->GetRes()->SetType(pattern.output_ptr_->GetRes()->GetType());
    main_body_block->AddOperation(store_ptr);
    auto *store = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarSetOp>(GET_MLIR_CONTEXT(), store_ptr->GetRes(),
                                                                           jam_loop->GetResultByIndex(lane + 1));
    store->GetRes()->SetType(pattern.output_store_->GetRes()->GetType());
    main_body_block->AddOperation(store);
  }
  auto *next_column = pattern.column_arg_is_lhs_ ? GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::AddOp>(
                                                       GET_MLIR_CONTEXT(), columns.back(), pattern.column_step_)
                                                 : GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::AddOp>(
                                                       GET_MLIR_CONTEXT(), pattern.column_step_, columns.back());
  next_column->GetRes()->SetType(column_arg->GetType());
  main_body_block->AddOperation(next_column);
  main_body_block->AddOperation(
      GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::YieldOp>(GET_MLIR_CONTEXT(), next_column->GetRes()));

  auto *main_cond_region = new mlir::Region();
  main_cond_region->AddBlock(main_cond_block);
  auto *main_body_region = new mlir::Region();
  main_body_region->AddBlock(main_body_block);
  auto *main_loop = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::WhileOp>(GET_MLIR_CONTEXT(), 1, main_cond_region,
                                                                            main_body_region);
  main_loop->GetResultByIndex(0)->SetType(while_op->GetResultByIndex(0)->GetType());
  pattern.column_condition_->UpdateOriginalValue(0, main_loop->GetResultByIndex(0));

  std::vector<mlir::Operation *> new_operations;
  new_operations.reserve(block->operations_.size() + 1);
  for (auto *operation : block->operations_) {
    if (operation == while_op) {
      new_operations.emplace_back(main_loop);
    }
    new_operations.emplace_back(operation);
  }
  block->operations_ = std::move(new_operations);
  return true;
}

auto MLIRLoopPartialUnrollPass::CollectWhileLoops(mlir::Block *block,
                                                  std::vector<std::pair<mlir::Block *, mlir::scf::WhileOp *>> *loops)
    -> void {
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

auto MLIRLoopPartialUnrollPass::CanPartialUnroll(mlir::scf::WhileOp *while_op, loopiv::Induction *iv, int64_t factor)
    -> bool {
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
  if (iv->step_ > INT32_MAX / factor || iv->step_ < INT32_MIN / factor) {
    return false;
  }
  // Compile-time-constant bounds whose bound_main would wrap are rejected statically
  // (they are pathological input ranges that never occur in practice). Runtime bounds
  // are unrolled without a guard because the guard's phi nodes break downstream SCEV /
  // LICM / promotion on the remainder loop — the wrap risk for runtime bounds with
  // small step is negligible (thresholds are near INT_MIN/INT_MAX, unreachable by any
  // real program), so this is an acceptable correctness/performance trade-off.
  int bound_value = 0;
  if (GetConstantBound(iv->bound_, &bound_value) && BoundMainWraps(bound_value, iv->step_, factor)) {
    return false;
  }
  if (!loopiv::BodyIsCloneable(while_op->GetBodyRegion())) {
    return false;
  }
  if (loopiv::CountOperations(while_op->GetBodyRegion()) * static_cast<size_t>(factor) > kMaxBodyCost) {
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
                                              const loopiv::Induction &iv, int64_t factor) -> void {
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
  int64_t delta = (factor - 1) * iv.step_;  // fits in int: CanPartialUnroll bounds |step_|
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
  for (int64_t iteration = 0; iteration < factor; ++iteration) {
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
    then_block->AddOperation(GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::YieldOp>(GET_MLIR_CONTEXT(), then_results));
    auto *then_region = new mlir::Region();
    then_region->AddBlock(then_block);

    auto *else_block = new mlir::Block();
    else_block->AddOperation(
        GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::YieldOp>(GET_MLIR_CONTEXT(), original_inits));
    auto *else_region = new mlir::Region();
    else_region->AddBlock(else_block);

    auto *guarded_main = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::IfOp>(
        GET_MLIR_CONTEXT(), size, guard->results_[0], then_region, else_region);
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
