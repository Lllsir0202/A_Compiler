#include "middleend/pass/transform/mlirfixedstatetransition/mlirfixedstatetransition.h"

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "context/compiler_context.h"
#include "context/pass_context.h"
#include "middleend/mlir/dialect/arithm/arithm.h"
#include "middleend/mlir/dialect/func/funccall.h"
#include "middleend/mlir/dialect/func/funcdef.h"
#include "middleend/mlir/dialect/func/return.h"
#include "middleend/mlir/dialect/scf/scf.h"
#include "middleend/mlir/dialect/var/var.h"
#include "middleend/mlir/utils/clone/uselistrebuilder.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/block.h"
#include "middleend/mlir/utils/struct/region.h"
#include "middleend/mlir/utils/value/value.h"
#include "middleend/pass/analysis/mlirmemoryeffects/mlirmemoryeffects.h"
#include "middleend/pass/analysis/sideeffect/sideeffect.h"
#include "middleend/pass/transform/mlirloopunroll/inductionvariable.h"
#include "utils/type/type.h"

#define GET_MLIR_CONTEXT() compiler_context_->GetIRContext()->GetMLIRContext()
#define GET_TYPE_CONTEXT() compiler_context_->GetTypeContext()

namespace middleend::pass::transform {
namespace {

using analysis::MLIRMemoryEffectAnalysis;
using analysis::SideEffectSummary;

struct AccessEvent {
  bool reads{false};
  bool writes{false};
  bool direct_store{false};
  mlir::Value *stored_value{nullptr};
};

auto IsScalarIntegerGlobal(mlir::Value *root) -> bool {
  auto *global = root == nullptr ? nullptr : mlir::DynamicOrNull<mlir::var::GlobalVarDefOp>(root->op_);
  if (global == nullptr || !global->GetDimensions().empty() || root->GetType() == nullptr) {
    return false;
  }
  return root->GetType()->GetKind() == type::TypeKind::INT;
}

auto IsAllowedBodyOperation(mlir::Operation *operation) -> bool {
  return mlir::DynamicOrNull<mlir::arithm::ArithmOps>(operation) != nullptr ||
         mlir::DynamicOrNull<mlir::var::VarGetPtrOp>(operation) != nullptr ||
         mlir::DynamicOrNull<mlir::var::VarGetValOp>(operation) != nullptr ||
         mlir::DynamicOrNull<mlir::var::VarSetOp>(operation) != nullptr ||
         mlir::DynamicOrNull<mlir::func::FuncCallOp>(operation) != nullptr ||
         mlir::DynamicOrNull<mlir::scf::YieldOp>(operation) != nullptr;
}

auto EffectHasPointerArguments(const SideEffectSummary &summary) -> bool {
  return std::any_of(summary.arg_effects.begin(), summary.arg_effects.end(),
                     [](SideEffectSummary::ArgEffect effect) { return effect != SideEffectSummary::ArgEffect::kNone; });
}

auto RootOf(mlir::Value *pointer) -> mlir::Value * {
  auto root = MLIRMemoryEffectAnalysis::GetPointerRoot(pointer);
  return root.kind == MLIRMemoryEffectAnalysis::RootKind::kGlobal ? root.value : nullptr;
}

auto MakeYieldRegion(context::MLIRContext *context, mlir::Value *value) -> mlir::Region * {
  auto *region = new mlir::Region();
  auto *block = new mlir::Block();
  block->operations_.emplace_back(context->CreateOperation<mlir::scf::YieldOp>(context, value));
  region->blocks_.emplace_back(block);
  return region;
}

auto DependsOn(mlir::Value *value, mlir::Value *argument, mlir::scf::WhileOp *loop,
               std::unordered_map<mlir::Value *, bool> *memo, std::unordered_set<mlir::Value *> *visiting) -> bool {
  if (value == argument) return true;
  auto found = memo->find(value);
  if (found != memo->end()) return found->second;
  if (value == nullptr || !loopiv::IsDefinedInLoop(value, loop) || value->op_ == nullptr ||
      !visiting->insert(value).second) {
    (*memo)[value] = false;
    return false;
  }
  bool dependent = false;
  for (auto *operand : value->op_->operands_) {
    dependent = dependent || DependsOn(operand, argument, loop, memo, visiting);
  }
  visiting->erase(value);
  (*memo)[value] = dependent;
  return dependent;
}

auto DependsOn(mlir::Value *value, mlir::Value *argument, mlir::scf::WhileOp *loop) -> bool {
  std::unordered_map<mlir::Value *, bool> memo;
  std::unordered_set<mlir::Value *> visiting;
  return DependsOn(value, argument, loop, &memo, &visiting);
}

auto CollectDefiningOperations(mlir::Value *value, mlir::Value *stop, mlir::scf::WhileOp *loop,
                               std::unordered_set<mlir::Operation *> *operations) -> void {
  if (value == nullptr || value == stop || !loopiv::IsDefinedInLoop(value, loop) || value->op_ == nullptr ||
      !operations->insert(value->op_).second) {
    return;
  }
  for (auto *operand : value->op_->operands_) {
    CollectDefiningOperations(operand, stop, loop, operations);
  }
}

}  // namespace

auto MLIRFixedStateTransitionPass::Run() -> void {
  PreRun();
  memory_effects_ =
      compiler_context_->GetPassContext()->GetSpecPass<analysis::MLIRMemoryEffectAnalysis>("MLIRMemoryEffectAnalysis");
  if (memory_effects_ == nullptr) {
    throw std::logic_error("MLIRFixedStateTransitionPass requires MLIRMemoryEffectAnalysis");
  }

  loops_seen_ = 0;
  candidates_ = 0;
  transformed_ = 0;
  effect_rejects_ = 0;
  state_rejects_ = 0;
  dependence_rejects_ = 0;
  if (std::getenv("FIXED_STATE_TRANSITION_OFF") == nullptr) {
    auto *module = GET_MLIR_CONTEXT()->GetModuleOp();
    if (module == nullptr) {
      throw std::logic_error("Module operation is not set in MLIRContext");
    }
    for (auto *operation : module->GetBlock()->operations_) {
      if (auto *function = mlir::DynamicOrNull<mlir::func::FuncDefOp>(operation)) {
        RunOnFunction(function);
      }
    }
  }
  if (std::getenv("LOOP_OPT_STATS") != nullptr || std::getenv("FIXED_STATE_TRANSITION_STATS") != nullptr) {
    std::cerr << "FixedStateTransition: loops=" << loops_seen_ << " candidates=" << candidates_
              << " transformed=" << transformed_ << " effects=" << effect_rejects_ << " state=" << state_rejects_
              << " dependence=" << dependence_rejects_ << '\n';
  }
}

auto MLIRFixedStateTransitionPass::RunOnFunction(mlir::func::FuncDefOp *function) -> bool {
  return TransformInBlock(function, function->GetEntryBlock());
}

auto MLIRFixedStateTransitionPass::TransformInBlock(mlir::func::FuncDefOp *function, mlir::Block *block) -> bool {
  bool changed = false;
  for (auto *operation : block->operations_) {
    for (auto *region : operation->regions_) {
      for (auto *nested : region->blocks_) {
        changed = TransformInBlock(function, nested) || changed;
      }
    }
    auto *loop = mlir::DynamicOrNull<mlir::scf::WhileOp>(operation);
    if (loop == nullptr) continue;
    ++loops_seen_;
    Candidate candidate;
    if (!FindCandidate(loop, &candidate)) continue;
    ++candidates_;
    Transform(function, loop, candidate);
    ++transformed_;
    changed = true;
  }
  return changed;
}

auto MLIRFixedStateTransitionPass::FindCandidate(mlir::scf::WhileOp *loop, Candidate *candidate) -> bool {
  auto induction = loopiv::Recognize(loop);
  if (!induction.valid_ || induction.step_kind_ != loopiv::StepKind::kAdd ||
      (induction.kind_ != loopiv::CmpKind::kLT && induction.kind_ != loopiv::CmpKind::kGT &&
       induction.kind_ != loopiv::CmpKind::kNE) ||
      loopiv::ConstantTripCount(induction, 100000000) < 2 || loopiv::IsDefinedInLoop(induction.bound_, loop) ||
      !loop->GetResultByIndex(induction.index_)->GetUses().empty()) {
    ++dependence_rejects_;
    return false;
  }
  auto *condition_block = loop->GetCondRegion()->blocks_[0];
  auto *body = loop->GetBodyRegion()->blocks_[0];
  auto *yield = mlir::DynamicOrNull<mlir::scf::YieldOp>(body->operations_.back());
  if (condition_block->arguments_.size() != body->arguments_.size() || yield == nullptr ||
      yield->GetOperands().size() != body->arguments_.size()) {
    ++dependence_rejects_;
    return false;
  }

  // No carried value may influence the transition.  The sole exception is the
  // induction update itself; also reject any second use of the counter.
  std::unordered_set<mlir::Operation *> latch_operations;
  CollectDefiningOperations(yield->GetOperands()[induction.index_], body->arguments_[induction.index_], loop,
                            &latch_operations);
  for (size_t index = 0; index < body->arguments_.size(); ++index) {
    auto *argument = body->arguments_[index];
    for (auto *operation : body->operations_) {
      if (operation == yield) continue;
      bool dependent = false;
      for (auto *operand : operation->operands_) {
        dependent = dependent || DependsOn(operand, argument, loop);
      }
      if (!dependent) continue;
      if (index != induction.index_ || latch_operations.count(operation) == 0) {
        ++dependence_rejects_;
        return false;
      }
    }
    if (index != induction.index_ && DependsOn(yield->GetOperands()[index], argument, loop)) {
      ++dependence_rejects_;
      return false;
    }
  }

  std::unordered_map<mlir::Value *, std::vector<AccessEvent>> accesses;
  bool has_call = false;
  for (auto *operation : body->operations_) {
    if (!operation->regions_.empty() || !IsAllowedBodyOperation(operation)) {
      ++effect_rejects_;
      return false;
    }
    if (auto *load = mlir::DynamicOrNull<mlir::var::VarGetValOp>(operation)) {
      auto *root = RootOf(load->GetPtr());
      if (root == nullptr) {
        ++effect_rejects_;
        return false;
      }
      accesses[root].push_back({true, false, false, nullptr});
      continue;
    }
    if (auto *store = mlir::DynamicOrNull<mlir::var::VarSetOp>(operation)) {
      auto *root = RootOf(store->GetPtr());
      if (root == nullptr) {
        ++effect_rejects_;
        return false;
      }
      accesses[root].push_back({false, true, true, store->GetNewValue()});
      continue;
    }
    auto *call = mlir::DynamicOrNull<mlir::func::FuncCallOp>(operation);
    if (call == nullptr) continue;
    has_call = true;
    const auto &summary = memory_effects_->GetSummary(call->GetName());
    if (summary.reads_all_globals || summary.writes_all_globals || summary.reads_io || summary.writes_io ||
        summary.saves_pointer || EffectHasPointerArguments(summary)) {
      ++effect_rejects_;
      return false;
    }
    std::unordered_set<mlir::Value *> roots = summary.read_globals;
    roots.insert(summary.written_globals.begin(), summary.written_globals.end());
    for (auto *root : roots) {
      accesses[root].push_back(
          {summary.read_globals.count(root) != 0, summary.written_globals.count(root) != 0, false, nullptr});
    }
  }
  if (!has_call) {
    ++effect_rejects_;
    return false;
  }

  std::vector<mlir::Value *> state_roots;
  for (const auto &[root, events] : accesses) {
    bool reads = false;
    bool writes = false;
    for (const auto &event : events) {
      reads = reads || event.reads;
      writes = writes || event.writes;
    }
    if (!reads || !writes) continue;
    const auto &first = events.front();
    bool reset_each_iteration = first.direct_store && first.writes && !first.reads && first.stored_value != nullptr &&
                                !loopiv::IsDefinedInLoop(first.stored_value, loop);
    if (reset_each_iteration) continue;
    if (!IsScalarIntegerGlobal(root)) {
      if (std::getenv("FIXED_STATE_TRANSITION_STATS") != nullptr) {
        auto *global = mlir::DynamicOrNull<mlir::var::GlobalVarDefOp>(root->op_);
        std::cerr << "FixedStateTransition: reject non-scalar read/write global "
                  << (global == nullptr ? "<unknown>" : global->GetName()) << '\n';
      }
      ++state_rejects_;
      return false;
    }
    state_roots.emplace_back(root);
  }
  if (state_roots.empty()) {
    ++state_rejects_;
    return false;
  }
  std::sort(state_roots.begin(), state_roots.end(),
            [](mlir::Value *lhs, mlir::Value *rhs) { return lhs->num_ < rhs->num_; });

  candidate->induction_index = induction.index_;
  candidate->bound = induction.bound_;
  candidate->state_roots = std::move(state_roots);
  return true;
}

auto MLIRFixedStateTransitionPass::Transform(mlir::func::FuncDefOp *function, mlir::scf::WhileOp *loop,
                                             const Candidate &candidate) -> void {
  auto *body = loop->GetBodyRegion()->blocks_[0];
  auto *yield = mlir::DynamicOrNull<mlir::scf::YieldOp>(body->operations_.back());
  auto *context = GET_MLIR_CONTEXT();

  std::vector<mlir::Operation *> prefix;
  std::vector<mlir::Value *> before_values;
  for (auto *root : candidate.state_roots) {
    auto *pointer = context->CreateOperation<mlir::var::VarGetPtrOp>(context, root);
    pointer->GetRes()->SetType(GET_TYPE_CONTEXT()->GetPtrType(root->GetType()));
    auto *load = context->CreateOperation<mlir::var::VarGetValOp>(context, pointer->GetRes());
    load->GetRes()->SetType(root->GetType());
    prefix.emplace_back(pointer);
    prefix.emplace_back(load);
    before_values.emplace_back(load->GetRes());
  }
  body->operations_.insert(body->operations_.begin(), prefix.begin(), prefix.end());

  std::vector<mlir::Operation *> suffix;
  mlir::Value *stable = nullptr;
  type::Type *bool_type = nullptr;
  auto *condition = mlir::DynamicOrNull<mlir::scf::ConditionOp>(loop->GetCondRegion()->blocks_[0]->operations_.back());
  if (condition != nullptr && condition->GetCondition() != nullptr) {
    bool_type = condition->GetCondition()->GetType();
  }
  for (size_t index = 0; index < candidate.state_roots.size(); ++index) {
    auto *root = candidate.state_roots[index];
    auto *pointer = context->CreateOperation<mlir::var::VarGetPtrOp>(context, root);
    pointer->GetRes()->SetType(GET_TYPE_CONTEXT()->GetPtrType(root->GetType()));
    auto *load = context->CreateOperation<mlir::var::VarGetValOp>(context, pointer->GetRes());
    load->GetRes()->SetType(root->GetType());
    auto *equal = context->CreateOperation<mlir::arithm::EqualOp>(context, before_values[index], load->GetRes());
    equal->GetRes()->SetType(bool_type);
    suffix.emplace_back(pointer);
    suffix.emplace_back(load);
    suffix.emplace_back(equal);
    if (stable == nullptr) {
      stable = equal->GetRes();
    } else {
      auto *both = context->CreateOperation<mlir::scf::IfOp>(
          context, 1, stable, MakeYieldRegion(context, equal->GetRes()), MakeYieldRegion(context, stable));
      both->GetResultByIndex(0)->SetType(bool_type);
      suffix.emplace_back(both);
      stable = both->GetResultByIndex(0);
    }
  }

  auto &yield_values = yield->GetOperands();
  auto *next = yield_values[candidate.induction_index];
  auto *select_exit = context->CreateOperation<mlir::scf::IfOp>(
      context, 1, stable, MakeYieldRegion(context, candidate.bound), MakeYieldRegion(context, next));
  select_exit->GetResultByIndex(0)->SetType(next->GetType());
  suffix.emplace_back(select_exit);
  body->operations_.insert(body->operations_.end() - 1, suffix.begin(), suffix.end());
  yield_values[candidate.induction_index] = select_exit->GetResultByIndex(0);
  mlir::utils::RebuildFunctionUses(function);
}

}  // namespace middleend::pass::transform
