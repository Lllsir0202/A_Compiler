#include "middleend/pass/transform/mlirloopdomaintighten/mlirloopdomaintighten.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <utility>
#include <variant>
#include <vector>
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/arithm/binaryop.h"
#include "middleend/mlir/dialect/arithm/copy.h"
#include "middleend/mlir/dialect/arithm/unaryop.h"
#include "middleend/mlir/dialect/func/funcdef.h"
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
#define GET_TYPE_CONTEXT() compiler_context_->GetTypeContext()

namespace middleend::pass::transform {
namespace {

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
  if (value == nullptr) {
    return {false, 0};
  }
  auto *constant = mlir::DynamicOrNull<mlir::arithm::ConstOp>(value->op_);
  if (constant == nullptr || !std::holds_alternative<int>(constant->GetValue())) {
    return {false, 0};
  }
  return {true, std::get<int>(constant->GetValue())};
}

auto IsIncrementByOne(mlir::Value *value, mlir::Value *iv) -> bool {
  value = TraceCopies(value);
  auto *add = value == nullptr ? nullptr : mlir::DynamicOrNull<mlir::arithm::AddOp>(value->op_);
  if (add == nullptr) {
    return false;
  }
  auto [lhs_constant, lhs] = TraceConstInt(add->GetLHS());
  auto [rhs_constant, rhs] = TraceConstInt(add->GetRHS());
  return (TraceCopies(add->GetLHS()) == iv && rhs_constant && rhs == 1) ||
         (TraceCopies(add->GetRHS()) == iv && lhs_constant && lhs == 1);
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

// The selected branch must be semantically just:
//   next = iv + 1; continue(next); yield(next)
// Copy operations and the add itself are the only allowed definitions.
auto IsEmptyContinueRegion(mlir::Region *region, mlir::Value *iv, mlir::Value **next) -> bool {
  auto *yield = SingleYield(region);
  if (yield == nullptr || yield->GetOperands().size() != 1) {
    return false;
  }
  mlir::scf::ContinueOp *continue_op = nullptr;
  for (auto *operation : region->blocks_[0]->operations_) {
    if (operation == yield) {
      continue;
    }
    if (auto *candidate = mlir::DynamicOrNull<mlir::scf::ContinueOp>(operation)) {
      if (continue_op != nullptr) {
        return false;
      }
      continue_op = candidate;
      continue;
    }
    if (mlir::DynamicOrNull<mlir::arithm::AddOp>(operation) == nullptr &&
        mlir::DynamicOrNull<mlir::arithm::CopyOp>(operation) == nullptr) {
      return false;
    }
  }
  if (continue_op == nullptr || continue_op->GetOperands().size() != 1 ||
      !IsIncrementByOne(continue_op->GetOperands()[0], iv) ||
      TraceCopies(yield->GetOperands()[0]) != TraceCopies(continue_op->GetOperands()[0])) {
    return false;
  }
  *next = continue_op->GetOperands()[0];
  return true;
}

}  // namespace

auto MLIRLoopDomainTightenPass::Run() -> void {
  PreRun();
  // Keep deterministic A/B measurement available without changing the pass
  // pipeline or rebuilding the compiler.
  if (std::getenv("LOOP_DOMAIN_TIGHTEN_OFF") != nullptr) {
    return;
  }
  loops_seen_ = 0;
  candidates_ = 0;
  tightened_ = 0;
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
  if (std::getenv("LOOP_OPT_STATS") != nullptr) {
    std::cerr << "LoopDomainTighten: candidates=" << candidates_ << " tightened=" << tightened_
              << " loops=" << loops_seen_ << '\n';
  }
}

auto MLIRLoopDomainTightenPass::RunOnFunction(mlir::func::FuncDefOp *func_op) -> bool {
  mlir::utils::RebuildFunctionUses(func_op);
  bool changed = TightenInBlock(func_op, func_op->GetEntryBlock());
  if (changed) {
    mlir::utils::RebuildFunctionUses(func_op);
  }
  return changed;
}

auto MLIRLoopDomainTightenPass::TightenInBlock(mlir::func::FuncDefOp *func_op, mlir::Block *block) -> bool {
  bool changed = false;
  // Process inner control flow first, so a candidate is never revisited through
  // the helper scf.if operations created by this pass.
  std::vector<mlir::Operation *> original_operations = block->operations_;
  for (auto *operation : original_operations) {
    for (auto *region : operation->regions_) {
      for (auto *nested : region->blocks_) {
        changed = TightenInBlock(func_op, nested) || changed;
      }
    }
  }

  for (size_t index = 0; index < block->operations_.size(); ++index) {
    auto *loop = mlir::DynamicOrNull<mlir::scf::WhileOp>(block->operations_[index]);
    if (loop == nullptr) {
      continue;
    }
    Candidate candidate;
    if (!FindEmptySuffix(loop, &candidate)) {
      continue;
    }
    ++candidates_;
    Tighten(func_op, block, index, loop, candidate);
    ++tightened_;
    changed = true;
    index += 6;
  }
  return changed;
}

auto MLIRLoopDomainTightenPass::FindEmptySuffix(mlir::scf::WhileOp *loop, Candidate *candidate) -> bool {
  ++loops_seen_;
  if (loop->GetResultsSize() != 1 || loop->GetCondRegion()->blocks_.size() != 1 ||
      loop->GetBodyRegion()->blocks_.size() != 1) {
    return false;
  }

  auto *cond_block = loop->GetCondRegion()->blocks_[0];
  auto *body = loop->GetBodyRegion()->blocks_[0];
  if (cond_block->arguments_.size() != 1 || body->arguments_.size() != 1 || cond_block->operations_.empty() ||
      body->operations_.empty()) {
    return false;
  }
  auto *iv = body->arguments_[0];
  auto *condition = mlir::DynamicOrNull<mlir::scf::ConditionOp>(cond_block->operations_.back());
  auto original_values = condition == nullptr ? std::vector<mlir::Value *>{} : condition->GetOriginalValues();
  auto *condition_value = condition == nullptr ? nullptr : TraceCopies(condition->GetCondition());
  auto *less = condition_value == nullptr ? nullptr : mlir::DynamicOrNull<mlir::arithm::LessOp>(condition_value->op_);
  if (less == nullptr || original_values.size() != 1 || TraceCopies(less->GetLHS()) != cond_block->arguments_[0] ||
      loopiv::IsDefinedInLoop(TraceCopies(less->GetRHS()), loop)) {
    return false;
  }
  auto *bound = TraceCopies(less->GetRHS());

  for (size_t index = 0; index + 1 < body->operations_.size(); ++index) {
    auto *operation = body->operations_[index];
    auto *if_op = mlir::DynamicOrNull<mlir::scf::IfOp>(operation);
    if (if_op == nullptr) {
      // Comparisons/copies used to form the guard are non-trapping. Do not
      // move the boundary across general arithmetic or memory operations.
      if (mlir::DynamicOrNull<mlir::arithm::CopyOp>(operation) == nullptr &&
          mlir::DynamicOrNull<mlir::arithm::LessOp>(operation) == nullptr) {
        return false;
      }
      continue;
    }
    if (if_op->results_.size() != 1) {
      return false;
    }
    auto *guard = mlir::DynamicOrNull<mlir::arithm::LessOp>(TraceCopies(if_op->GetCondition())->op_);
    if (guard == nullptr || TraceCopies(guard->GetRHS()) != iv) {
      return false;
    }
    auto *cut = TraceCopies(guard->GetLHS());
    if (cut == nullptr || loopiv::IsDefinedInLoop(cut, loop)) {
      return false;
    }

    mlir::Value *next = nullptr;
    auto *else_yield = SingleYield(if_op->GetElseRegion());
    if (!IsEmptyContinueRegion(if_op->GetThenRegion(), iv, &next) || else_yield == nullptr ||
        else_yield->GetOperands().size() != 1 || TraceCopies(else_yield->GetOperands()[0]) != iv ||
        TraceCopies(if_op->GetThenRegion()->blocks_[0]->operations_.back()->operands_[0]) != TraceCopies(next)) {
      return false;
    }
    auto *body_yield = mlir::DynamicOrNull<mlir::scf::YieldOp>(body->operations_.back());
    if (body_yield == nullptr || body_yield->GetOperands().size() != 1 ||
        !IsIncrementByOne(body_yield->GetOperands()[0], if_op->GetResultByIndex(0))) {
      return false;
    }
    candidate->skip_if = if_op;
    candidate->body_index = index;
    candidate->cut = cut;
    candidate->init = original_values[0];
    candidate->bound = bound;
    return true;
  }
  return false;
}

auto MLIRLoopDomainTightenPass::Tighten(mlir::func::FuncDefOp *func_op, mlir::Block *parent, size_t loop_index,
                                        mlir::scf::WhileOp *loop, const Candidate &candidate) -> void {
  auto *body = loop->GetBodyRegion()->blocks_[0];
  auto *cond_block = loop->GetCondRegion()->blocks_[0];
  auto *condition = mlir::DynamicOrNull<mlir::scf::ConditionOp>(cond_block->operations_.back());
  auto *condition_less = mlir::DynamicOrNull<mlir::arithm::LessOp>(TraceCopies(condition->GetCondition())->op_);
  auto *integer_type = candidate.bound->GetType();
  auto *bool_type = condition->GetCondition()->GetType();

  auto *one = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::ConstOp>(GET_MLIR_CONTEXT(), 1);
  one->GetRes()->SetType(integer_type);
  auto *cut_less_bound =
      GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::LessOp>(GET_MLIR_CONTEXT(), candidate.cut, candidate.bound);
  cut_less_bound->GetRes()->SetType(bool_type);
  auto *cut_plus_one =
      GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::AddOp>(GET_MLIR_CONTEXT(), candidate.cut, one->GetRes());
  cut_plus_one->GetRes()->SetType(integer_type);
  auto *tight_bound = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::IfOp>(
      GET_MLIR_CONTEXT(), 1, cut_less_bound->GetRes(), MakeYieldRegion(GET_MLIR_CONTEXT(), cut_plus_one->GetRes()),
      MakeYieldRegion(GET_MLIR_CONTEXT(), candidate.bound));
  tight_bound->GetResultByIndex(0)->SetType(integer_type);

  auto *init_less_bound =
      GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::LessOp>(GET_MLIR_CONTEXT(), candidate.init, candidate.bound);
  init_less_bound->GetRes()->SetType(bool_type);
  auto *original_exit = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::IfOp>(
      GET_MLIR_CONTEXT(), 1, init_less_bound->GetRes(), MakeYieldRegion(GET_MLIR_CONTEXT(), candidate.bound),
      MakeYieldRegion(GET_MLIR_CONTEXT(), candidate.init));
  original_exit->GetResultByIndex(0)->SetType(integer_type);

  condition_less->operands_[1] = tight_bound->GetResultByIndex(0);
  loop->GetResultByIndex(0)->ReplaceAllUsesWith(original_exit->GetResultByIndex(0));
  candidate.skip_if->GetResultByIndex(0)->ReplaceAllUsesWith(body->arguments_[0]);
  body->operations_.erase(body->operations_.begin() + static_cast<std::ptrdiff_t>(candidate.body_index));

  std::vector<mlir::Operation *> helpers = {one,         cut_less_bound,  cut_plus_one,
                                            tight_bound, init_less_bound, original_exit};
  parent->operations_.insert(parent->operations_.begin() + static_cast<std::ptrdiff_t>(loop_index), helpers.begin(),
                             helpers.end());
  mlir::utils::RebuildFunctionUses(func_op);
}

}  // namespace middleend::pass::transform
