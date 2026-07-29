#include "middleend/pass/transform/mlirloopinvariantrepeat/mlirloopinvariantrepeat.h"
#include <cstdlib>
#include <iostream>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/arithm/binaryop.h"
#include "middleend/mlir/dialect/arithm/copy.h"
#include "middleend/mlir/dialect/arithm/unaryop.h"
#include "middleend/mlir/dialect/func/funccall.h"
#include "middleend/mlir/dialect/func/funcdef.h"
#include "middleend/mlir/dialect/scf/break.h"
#include "middleend/mlir/dialect/scf/condition.h"
#include "middleend/mlir/dialect/scf/continue.h"
#include "middleend/mlir/dialect/scf/if.h"
#include "middleend/mlir/dialect/scf/while.h"
#include "middleend/mlir/dialect/scf/yield.h"
#include "middleend/mlir/dialect/var/set.h"
#include "middleend/mlir/utils/clone/uselistrebuilder.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/block.h"
#include "middleend/mlir/utils/struct/region.h"
#include "middleend/mlir/utils/value/value.h"
#include "middleend/pass/transform/mlirloopunroll/inductionvariable.h"
#include "utils/type/type.h"

#define GET_MLIR_CONTEXT() compiler_context_->GetIRContext()->GetMLIRContext()

namespace middleend::pass::transform {
namespace {

struct Dependency {
  bool accumulator{false};
  bool induction{false};
};

auto Merge(Dependency lhs, Dependency rhs) -> Dependency {
  return {lhs.accumulator || rhs.accumulator, lhs.induction || rhs.induction};
}

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

auto TraceConstInt(mlir::Value *value) -> std::pair<bool, int> {
  value = TraceCopies(value);
  auto *constant = value == nullptr ? nullptr : mlir::DynamicOrNull<mlir::arithm::ConstOp>(value->op_);
  if (constant == nullptr || !std::holds_alternative<int>(constant->GetValue())) {
    return {false, 0};
  }
  return {true, std::get<int>(constant->GetValue())};
}

auto SingleYield(mlir::Region *region) -> mlir::scf::YieldOp * {
  if (region == nullptr || region->blocks_.size() != 1 || region->blocks_[0]->operations_.empty()) {
    return nullptr;
  }
  return mlir::DynamicOrNull<mlir::scf::YieldOp>(region->blocks_[0]->operations_.back());
}

auto MakeYieldRegion(context::MLIRContext *context, mlir::Value *value) -> mlir::Region * {
  auto *region = new mlir::Region();
  auto *block = new mlir::Block();
  block->operations_.emplace_back(context->CreateOperation<mlir::scf::YieldOp>(context, value));
  region->blocks_.emplace_back(block);
  return region;
}

auto CollectOperations(mlir::Region *region, std::vector<mlir::Operation *> *operations,
                       std::vector<mlir::scf::WhileOp *> *loops) -> void {
  if (region == nullptr) {
    return;
  }
  for (auto *block : region->blocks_) {
    for (auto *operation : block->operations_) {
      operations->emplace_back(operation);
      if (auto *loop = mlir::DynamicOrNull<mlir::scf::WhileOp>(operation)) {
        loops->emplace_back(loop);
      }
      for (auto *nested : operation->regions_) {
        CollectOperations(nested, operations, loops);
      }
    }
  }
}

auto ComputeDependencies(mlir::scf::WhileOp *outer, size_t accumulator_index, size_t induction_index,
                         std::unordered_map<mlir::Value *, Dependency> *dependencies,
                         std::vector<mlir::Operation *> *operations) -> bool {
  auto *outer_cond = outer->GetCondRegion()->blocks_[0];
  auto *outer_body = outer->GetBodyRegion()->blocks_[0];
  if (outer_cond->arguments_.size() != outer_body->arguments_.size()) {
    return false;
  }
  (*dependencies)[outer_cond->arguments_[accumulator_index]].accumulator = true;
  (*dependencies)[outer_body->arguments_[accumulator_index]].accumulator = true;
  (*dependencies)[outer_cond->arguments_[induction_index]].induction = true;
  (*dependencies)[outer_body->arguments_[induction_index]].induction = true;

  std::vector<mlir::scf::WhileOp *> nested_loops;
  CollectOperations(outer->GetCondRegion(), operations, &nested_loops);
  CollectOperations(outer->GetBodyRegion(), operations, &nested_loops);

  auto get = [&](mlir::Value *value) {
    auto iter = dependencies->find(value);
    return iter == dependencies->end() ? Dependency{} : iter->second;
  };
  auto update = [&](mlir::Value *value, Dependency dependency) {
    auto old = get(value);
    auto merged = Merge(old, dependency);
    bool changed = old.accumulator != merged.accumulator || old.induction != merged.induction;
    (*dependencies)[value] = merged;
    return changed;
  };

  bool changed = true;
  size_t limit = operations->size() * 4 + 8;
  for (size_t iteration = 0; changed && iteration < limit; ++iteration) {
    changed = false;
    for (auto *loop : nested_loops) {
      if (loop->GetCondRegion()->blocks_.size() != 1 || loop->GetBodyRegion()->blocks_.size() != 1) {
        return false;
      }
      auto *cond_block = loop->GetCondRegion()->blocks_[0];
      auto *body_block = loop->GetBodyRegion()->blocks_[0];
      auto *condition = cond_block->operations_.empty()
                            ? nullptr
                            : mlir::DynamicOrNull<mlir::scf::ConditionOp>(cond_block->operations_.back());
      auto *yield = SingleYield(loop->GetBodyRegion());
      auto original = condition == nullptr ? std::vector<mlir::Value *>{} : condition->GetOriginalValues();
      if (yield == nullptr || original.size() != cond_block->arguments_.size() ||
          original.size() != body_block->arguments_.size() || original.size() != loop->results_.size() ||
          yield->GetOperands().size() != original.size()) {
        return false;
      }
      for (size_t index = 0; index < original.size(); ++index) {
        Dependency dependency = Merge(get(original[index]), get(yield->GetOperands()[index]));
        changed = update(cond_block->arguments_[index], dependency) || changed;
        changed = update(body_block->arguments_[index], dependency) || changed;
        changed = update(loop->GetResultByIndex(index), dependency) || changed;
      }
    }

    for (auto *operation : *operations) {
      Dependency dependency;
      for (auto *operand : operation->operands_) {
        dependency = Merge(dependency, get(operand));
      }
      if (auto *if_op = mlir::DynamicOrNull<mlir::scf::IfOp>(operation)) {
        auto *then_yield = SingleYield(if_op->GetThenRegion());
        auto *else_yield = SingleYield(if_op->GetElseRegion());
        if (then_yield == nullptr || else_yield == nullptr ||
            then_yield->GetOperands().size() != operation->results_.size() ||
            else_yield->GetOperands().size() != operation->results_.size()) {
          return false;
        }
        for (size_t index = 0; index < operation->results_.size(); ++index) {
          auto result_dependency =
              Merge(dependency, Merge(get(then_yield->GetOperands()[index]), get(else_yield->GetOperands()[index])));
          changed = update(operation->results_[index], result_dependency) || changed;
        }
        continue;
      }
      if (mlir::DynamicOrNull<mlir::scf::WhileOp>(operation) != nullptr) {
        continue;
      }
      for (auto *result : operation->results_) {
        changed = update(result, dependency) || changed;
      }
    }
  }
  return !changed;
}

auto HasForbiddenEffect(mlir::Operation *operation) -> bool {
  return mlir::DynamicOrNull<mlir::var::VarSetOp>(operation) != nullptr ||
         mlir::DynamicOrNull<mlir::func::FuncCallOp>(operation) != nullptr ||
         mlir::DynamicOrNull<mlir::scf::ContinueOp>(operation) != nullptr ||
         mlir::DynamicOrNull<mlir::scf::BreakOp>(operation) != nullptr;
}

}  // namespace

auto MLIRLoopInvariantRepeatPass::Run() -> void {
  PreRun();
  if (std::getenv("LOOP_INVARIANT_REPEAT_OFF") != nullptr) {
    return;
  }
  loops_seen_ = 0;
  candidates_ = 0;
  folded_ = 0;
  auto *module = GET_MLIR_CONTEXT()->GetModuleOp();
  if (module == nullptr) {
    throw std::logic_error("Module operation is not set in MLIRContext");
  }
  for (auto *operation : module->GetBlock()->operations_) {
    auto *function = mlir::DynamicOrNull<mlir::func::FuncDefOp>(operation);
    if (function != nullptr) {
      RunOnFunction(function);
    }
  }
  if (std::getenv("LOOP_OPT_STATS") != nullptr || std::getenv("LOOP_REPEAT_STATS") != nullptr) {
    std::cerr << "LoopInvariantRepeat: candidates=" << candidates_ << " folded=" << folded_ << " loops=" << loops_seen_
              << '\n';
  }
}

auto MLIRLoopInvariantRepeatPass::RunOnFunction(mlir::func::FuncDefOp *function) -> bool {
  mlir::utils::RebuildFunctionUses(function);
  bool changed = FoldInBlock(function, function->GetEntryBlock());
  if (changed) {
    mlir::utils::RebuildFunctionUses(function);
  }
  return changed;
}

auto MLIRLoopInvariantRepeatPass::FoldInBlock(mlir::func::FuncDefOp *function, mlir::Block *block) -> bool {
  bool changed = false;
  std::vector<mlir::Operation *> original = block->operations_;
  for (auto *operation : original) {
    for (auto *region : operation->regions_) {
      for (auto *nested : region->blocks_) {
        changed = FoldInBlock(function, nested) || changed;
      }
    }
  }

  for (size_t index = 0; index < block->operations_.size(); ++index) {
    auto *loop = mlir::DynamicOrNull<mlir::scf::WhileOp>(block->operations_[index]);
    if (loop == nullptr) {
      continue;
    }
    Candidate candidate;
    if (!FindCandidate(loop, &candidate)) {
      continue;
    }
    ++candidates_;
    Fold(function, block, index, loop, candidate);
    ++folded_;
    changed = true;
    // Five helper operations were inserted before the loop. Skip the shifted
    // loop itself so it cannot be folded repeatedly in the same traversal.
    index += 5;
  }
  return changed;
}

auto MLIRLoopInvariantRepeatPass::FindCandidate(mlir::scf::WhileOp *loop, Candidate *candidate) -> bool {
  ++loops_seen_;
  auto induction = loopiv::Recognize(loop);
  auto [zero_init, init] = TraceConstInt(induction.init_);
  if (!induction.valid_ || induction.kind_ != loopiv::CmpKind::kLT || induction.step_kind_ != loopiv::StepKind::kAdd ||
      induction.step_ != 1 || !zero_init || init != 0 || loopiv::IsDefinedInLoop(induction.bound_, loop) ||
      loop->GetCondRegion()->blocks_.size() != 1 || loop->GetBodyRegion()->blocks_.size() != 1) {
    return false;
  }
  auto *cond_block = loop->GetCondRegion()->blocks_[0];
  auto *body = loop->GetBodyRegion()->blocks_[0];
  auto *condition = cond_block->operations_.empty()
                        ? nullptr
                        : mlir::DynamicOrNull<mlir::scf::ConditionOp>(cond_block->operations_.back());
  auto *body_yield = SingleYield(loop->GetBodyRegion());
  auto original = condition == nullptr ? std::vector<mlir::Value *>{} : condition->GetOriginalValues();
  if (condition == nullptr || body_yield == nullptr || original.size() != loop->results_.size() ||
      original.size() != cond_block->arguments_.size() || original.size() != body->arguments_.size() ||
      body_yield->GetOperands().size() != original.size()) {
    return false;
  }

  size_t accumulator_index = original.size();
  for (size_t index = 0; index < loop->results_.size(); ++index) {
    if (index == induction.index_ || loop->GetResultByIndex(index)->GetUses().empty()) {
      continue;
    }
    if (accumulator_index != original.size()) {
      return false;
    }
    accumulator_index = index;
  }
  if (accumulator_index == original.size() ||
      original[accumulator_index]->GetType()->GetKind() != type::TypeKind::INT) {
    return false;
  }
  for (size_t index = 0; index < loop->results_.size(); ++index) {
    if (index != accumulator_index && !loop->GetResultByIndex(index)->GetUses().empty()) {
      return false;
    }
  }

  std::unordered_map<mlir::Value *, Dependency> dependencies;
  std::vector<mlir::Operation *> operations;
  if (!ComputeDependencies(loop, accumulator_index, induction.index_, &dependencies, &operations)) {
    return false;
  }
  auto dependency = [&](mlir::Value *value) {
    auto iter = dependencies.find(value);
    return iter == dependencies.end() ? Dependency{} : iter->second;
  };

  auto *condition_value = TraceCopies(condition->GetCondition());
  auto *condition_less =
      condition_value == nullptr ? nullptr : mlir::DynamicOrNull<mlir::arithm::LessOp>(condition_value->op_);
  auto *increment_value = TraceCopies(body_yield->GetOperands()[induction.index_]);
  auto *increment =
      increment_value == nullptr ? nullptr : mlir::DynamicOrNull<mlir::arithm::AddOp>(increment_value->op_);
  if (condition_less == nullptr || increment == nullptr ||
      !dependency(body_yield->GetOperands()[accumulator_index]).accumulator) {
    return false;
  }

  for (auto *operation : operations) {
    if (HasForbiddenEffect(operation)) {
      return false;
    }
    bool accumulator_operand = false;
    bool induction_operand = false;
    for (auto *operand : operation->operands_) {
      accumulator_operand = accumulator_operand || dependency(operand).accumulator;
      induction_operand = induction_operand || dependency(operand).induction;
    }

    if (induction_operand) {
      if (mlir::DynamicOrNull<mlir::arithm::CopyOp>(operation) != nullptr ||
          mlir::DynamicOrNull<mlir::scf::YieldOp>(operation) != nullptr ||
          mlir::DynamicOrNull<mlir::scf::ConditionOp>(operation) != nullptr) {
        continue;
      }
      if (operation == condition_less || operation == increment) {
        continue;
      }
      return false;
    }

    if (!accumulator_operand) {
      continue;
    }
    if (mlir::DynamicOrNull<mlir::arithm::CopyOp>(operation) != nullptr ||
        mlir::DynamicOrNull<mlir::scf::YieldOp>(operation) != nullptr ||
        mlir::DynamicOrNull<mlir::scf::ConditionOp>(operation) != nullptr) {
      continue;
    }
    if (auto *add = mlir::DynamicOrNull<mlir::arithm::AddOp>(operation)) {
      auto lhs = dependency(add->GetLHS());
      auto rhs = dependency(add->GetRHS());
      if (lhs.accumulator == rhs.accumulator || (lhs.accumulator ? rhs.induction : lhs.induction)) {
        return false;
      }
      continue;
    }
    if (auto *sub = mlir::DynamicOrNull<mlir::arithm::SubOp>(operation)) {
      auto lhs = dependency(sub->GetLHS());
      auto rhs = dependency(sub->GetRHS());
      if (!lhs.accumulator || rhs.accumulator || rhs.induction) {
        return false;
      }
      continue;
    }
    return false;
  }

  candidate->induction_index = induction.index_;
  candidate->accumulator_index = accumulator_index;
  candidate->bound = induction.bound_;
  candidate->accumulator_init = original[accumulator_index];
  candidate->condition_compare = condition_less;
  return true;
}

auto MLIRLoopInvariantRepeatPass::Fold(mlir::func::FuncDefOp *function, mlir::Block *parent, size_t loop_index,
                                       mlir::scf::WhileOp *loop, const Candidate &candidate) -> void {
  auto *integer_type = candidate.bound->GetType();
  auto *condition_compare = mlir::DynamicOrNull<mlir::arithm::LessOp>(candidate.condition_compare);
  auto *bool_type = condition_compare->GetRes()->GetType();

  auto *zero = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::ConstOp>(GET_MLIR_CONTEXT(), 0);
  auto *one = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::ConstOp>(GET_MLIR_CONTEXT(), 1);
  zero->GetRes()->SetType(integer_type);
  one->GetRes()->SetType(integer_type);
  auto *positive =
      GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::LessOp>(GET_MLIR_CONTEXT(), zero->GetRes(), candidate.bound);
  positive->GetRes()->SetType(bool_type);
  auto *trip_count = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::IfOp>(
      GET_MLIR_CONTEXT(), 1, positive->GetRes(), MakeYieldRegion(GET_MLIR_CONTEXT(), candidate.bound),
      MakeYieldRegion(GET_MLIR_CONTEXT(), zero->GetRes()));
  trip_count->GetResultByIndex(0)->SetType(integer_type);
  auto *one_trip_bound = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::IfOp>(
      GET_MLIR_CONTEXT(), 1, positive->GetRes(), MakeYieldRegion(GET_MLIR_CONTEXT(), one->GetRes()),
      MakeYieldRegion(GET_MLIR_CONTEXT(), zero->GetRes()));
  one_trip_bound->GetResultByIndex(0)->SetType(integer_type);

  condition_compare->operands_[1] = one_trip_bound->GetResultByIndex(0);
  std::vector<mlir::Operation *> before = {zero, one, positive, trip_count, one_trip_bound};
  parent->operations_.insert(parent->operations_.begin() + static_cast<std::ptrdiff_t>(loop_index), before.begin(),
                             before.end());

  auto *one_result = loop->GetResultByIndex(candidate.accumulator_index);
  auto *delta = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::SubOp>(GET_MLIR_CONTEXT(), one_result,
                                                                         candidate.accumulator_init);
  delta->GetRes()->SetType(integer_type);
  auto *scaled = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::MulOp>(GET_MLIR_CONTEXT(), delta->GetRes(),
                                                                          trip_count->GetResultByIndex(0));
  scaled->GetRes()->SetType(integer_type);
  auto *final = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::AddOp>(GET_MLIR_CONTEXT(), candidate.accumulator_init,
                                                                         scaled->GetRes());
  final->GetRes()->SetType(integer_type);
  parent->operations_.insert(parent->operations_.begin() + static_cast<std::ptrdiff_t>(loop_index + before.size() + 1),
                             {delta, scaled, final});

  // The new operations are not in the use lists yet, so this rewrites only
  // pre-existing consumers and cannot create a cycle through `delta`.
  one_result->ReplaceAllUsesWith(final->GetRes());
  mlir::utils::RebuildFunctionUses(function);
}

}  // namespace middleend::pass::transform
