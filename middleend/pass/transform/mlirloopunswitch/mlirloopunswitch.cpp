#include "middleend/pass/transform/mlirloopunswitch/mlirloopunswitch.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <unordered_map>
#include <variant>
#include <vector>
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/arithm/binaryop.h"
#include "middleend/mlir/dialect/arithm/copy.h"
#include "middleend/mlir/dialect/arithm/unaryop.h"
#include "middleend/mlir/dialect/func/funcdef.h"
#include "middleend/mlir/dialect/func/return.h"
#include "middleend/mlir/dialect/scf/condition.h"
#include "middleend/mlir/dialect/scf/if.h"
#include "middleend/mlir/dialect/scf/while.h"
#include "middleend/mlir/dialect/scf/yield.h"
#include "middleend/mlir/dialect/var/getptr.h"
#include "middleend/mlir/dialect/var/getvalue.h"
#include "middleend/mlir/utils/clone/uselistrebuilder.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/block.h"
#include "middleend/mlir/utils/struct/region.h"
#include "middleend/mlir/utils/value/value.h"
#include "middleend/pass/analysis/mlirmemoryeffects/mlirmemoryeffects.h"
#include "middleend/pass/transform/mlirloopunroll/inductionvariable.h"

#define GET_MLIR_CONTEXT() compiler_context_->GetIRContext()->GetMLIRContext()
#define GET_TYPE_CONTEXT() compiler_context_->GetTypeContext()

namespace middleend::pass::transform {
namespace {

auto IsConstant(mlir::Value *value) -> bool {
  return value != nullptr && mlir::DynamicOrNull<mlir::arithm::ConstOp>(value->op_) != nullptr;
}

auto RegionCanSpecialize(mlir::Region *region, size_t result_count) -> bool {
  if (region == nullptr || region->blocks_.size() != 1 || region->blocks_[0]->operations_.empty()) {
    return false;
  }
  auto *yield = mlir::DynamicOrNull<mlir::scf::YieldOp>(region->blocks_[0]->operations_.back());
  return yield != nullptr && yield->GetOperands().size() == result_count;
}

auto IsPureInvariantOperation(mlir::Operation *operation) -> bool {
  return mlir::DynamicOrNull<mlir::arithm::CopyOp>(operation) != nullptr ||
         mlir::DynamicOrNull<mlir::arithm::AddOp>(operation) != nullptr ||
         mlir::DynamicOrNull<mlir::arithm::SubOp>(operation) != nullptr ||
         mlir::DynamicOrNull<mlir::arithm::MulOp>(operation) != nullptr ||
         mlir::DynamicOrNull<mlir::arithm::EqualOp>(operation) != nullptr ||
         mlir::DynamicOrNull<mlir::arithm::NotEqualOp>(operation) != nullptr ||
         mlir::DynamicOrNull<mlir::arithm::LessOp>(operation) != nullptr ||
         mlir::DynamicOrNull<mlir::arithm::LessEqualOp>(operation) != nullptr ||
         mlir::DynamicOrNull<mlir::arithm::GreaterOp>(operation) != nullptr ||
         mlir::DynamicOrNull<mlir::arithm::GreaterEqualOp>(operation) != nullptr ||
         mlir::DynamicOrNull<mlir::arithm::NegOp>(operation) != nullptr ||
         mlir::DynamicOrNull<mlir::arithm::NotOp>(operation) != nullptr;
}

auto BodyIsCloneableForUnswitch(mlir::Operation *operation) -> bool {
  if (mlir::DynamicOrNull<mlir::scf::WhileOp>(operation) != nullptr ||
      mlir::DynamicOrNull<mlir::scf::ConditionOp>(operation) != nullptr ||
      mlir::DynamicOrNull<mlir::func::ReturnOp>(operation) != nullptr ||
      mlir::DynamicOrNull<mlir::func::FuncDefOp>(operation) != nullptr) {
    return false;
  }
  for (auto *region : operation->regions_) {
    if (region == nullptr || region->blocks_.size() != 1) return false;
    for (auto *nested : region->blocks_[0]->operations_) {
      if (!BodyIsCloneableForUnswitch(nested)) return false;
    }
  }
  return true;
}

auto BodyIsCloneableForUnswitch(mlir::Region *region) -> bool {
  if (region == nullptr || region->blocks_.size() != 1) return false;
  return std::all_of(region->blocks_[0]->operations_.begin(), region->blocks_[0]->operations_.end(),
                     [](mlir::Operation *operation) { return BodyIsCloneableForUnswitch(operation); });
}

}  // namespace

auto MLIRLoopUnswitchPass::Run() -> void {
  PreRun();
  memory_effects_ =
      compiler_context_->GetPassContext()->GetSpecPass<analysis::MLIRMemoryEffectAnalysis>("MLIRMemoryEffectAnalysis");
  if (memory_effects_ == nullptr) {
    throw std::logic_error("MLIRLoopUnswitchPass requires MLIRMemoryEffectAnalysis");
  }
  candidates_ = 0;
  unswitched_loops_ = 0;
  loops_seen_ = 0;
  body_ifs_seen_ = 0;
  clone_or_cost_rejects_ = 0;
  invariant_rejects_ = 0;
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
    std::cerr << "LoopUnswitch: candidates=" << candidates_ << " unswitched=" << unswitched_loops_
              << " loops=" << loops_seen_ << " body_ifs=" << body_ifs_seen_ << " clone_cost=" << clone_or_cost_rejects_
              << " invariant=" << invariant_rejects_ << '\n';
  }
}

auto MLIRLoopUnswitchPass::RunOnFunction(mlir::func::FuncDefOp *func_op) -> bool {
  mlir::utils::RebuildFunctionUses(func_op);
  bool changed = UnswitchInBlock(func_op, func_op->GetEntryBlock());
  if (changed) {
    mlir::utils::RebuildFunctionUses(func_op);
  }
  return changed;
}

auto MLIRLoopUnswitchPass::UnswitchInBlock(mlir::func::FuncDefOp *func_op, mlir::Block *block) -> bool {
  bool changed = false;
  // Visit existing nested control flow first. Newly-created outer ifs are not
  // revisited, so one original loop is duplicated at most once in this pass.
  for (auto *operation : block->operations_) {
    if (mlir::DynamicOrNull<mlir::scf::WhileOp>(operation) != nullptr) {
      continue;
    }
    for (auto *region : operation->regions_) {
      for (auto *nested : region->blocks_) {
        changed = UnswitchInBlock(func_op, nested) || changed;
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
    Unswitch(func_op, block, index, loop, candidate);
    ++unswitched_loops_;
    changed = true;
  }
  return changed;
}

auto MLIRLoopUnswitchPass::FindCandidate(mlir::scf::WhileOp *loop, Candidate *candidate) -> bool {
  ++loops_seen_;
  if (!BodyIsCloneableForUnswitch(loop->GetBodyRegion()) ||
      loopiv::CountOperations(loop->GetCondRegion()) + loopiv::CountOperations(loop->GetBodyRegion()) > kMaxLoopCost ||
      loop->GetBodyRegion()->blocks_.size() != 1) {
    ++clone_or_cost_rejects_;
    return false;
  }
  auto *body = loop->GetBodyRegion()->blocks_[0];
  for (size_t index = 0; index + 1 < body->operations_.size(); ++index) {
    auto *if_op = mlir::DynamicOrNull<mlir::scf::IfOp>(body->operations_[index]);
    if (if_op != nullptr) {
      ++body_ifs_seen_;
    }
    std::unordered_set<mlir::Value *> visiting;
    bool whole_candidate = if_op != nullptr && !IsConstant(if_op->GetCondition()) &&
                           CanHoistInvariantCondition(loop, if_op->GetCondition(), &visiting) &&
                           RegionCanSpecialize(if_op->GetThenRegion(), if_op->results_.size()) &&
                           RegionCanSpecialize(if_op->GetElseRegion(), if_op->results_.size());
    if (whole_candidate) {
      candidate->body_index = index;
      candidate->if_op = if_op;
      candidate->condition = if_op->GetCondition();
      candidate->partial = false;
      return true;
    }

    if (if_op != nullptr && !IsConstant(if_op->GetCondition())) {
      visiting.clear();
      mlir::Value *invariant = nullptr;
      if (FindInvariantBoolean(loop, if_op->GetCondition(), &visiting, &invariant)) {
        candidate->body_index = index;
        candidate->if_op = if_op;
        candidate->condition = invariant;
        candidate->partial = true;
        return true;
      }
      ++invariant_rejects_;
    }
  }
  return false;
}

auto MLIRLoopUnswitchPass::FindInvariantBoolean(mlir::scf::WhileOp *loop, mlir::Value *value,
                                                std::unordered_set<mlir::Value *> *visiting,
                                                mlir::Value **candidate) -> bool {
  if (value == nullptr || value->GetType() == nullptr || !visiting->insert(value).second) {
    return false;
  }
  if (value->GetType()->GetKind() == type::TypeKind::BOOL && loopiv::IsDefinedInLoop(value, loop) &&
      !IsConstant(value)) {
    std::unordered_set<mlir::Value *> invariance_visiting;
    if (CanHoistInvariantCondition(loop, value, &invariance_visiting, true)) {
      *candidate = value;
      visiting->erase(value);
      return true;
    }
  }
  if (value->op_ != nullptr) {
    for (auto *operand : value->op_->operands_) {
      if (FindInvariantBoolean(loop, operand, visiting, candidate)) {
        visiting->erase(value);
        return true;
      }
    }
    for (auto *region : value->op_->regions_) {
      for (auto *block : region->blocks_) {
        for (auto *operation : block->operations_) {
          auto *yield = mlir::DynamicOrNull<mlir::scf::YieldOp>(operation);
          if (yield == nullptr) continue;
          for (auto *operand : yield->GetOperands()) {
            if (FindInvariantBoolean(loop, operand, visiting, candidate)) {
              visiting->erase(value);
              return true;
            }
          }
        }
      }
    }
  }
  visiting->erase(value);
  return false;
}

auto MLIRLoopUnswitchPass::CanHoistInvariantCondition(mlir::scf::WhileOp *loop, mlir::Value *value,
                                                      std::unordered_set<mlir::Value *> *visiting,
                                                      bool conditionally_executed) -> bool {
  if (!loopiv::IsDefinedInLoop(value, loop)) {
    return true;
  }
  if (value == nullptr || value->op_ == nullptr || !visiting->insert(value).second) {
    return false;
  }
  if (auto *load = mlir::DynamicOrNull<mlir::var::VarGetValOp>(value->op_)) {
    auto induction = loopiv::Recognize(loop);
    bool guaranteed_execution = !conditionally_executed && loopiv::ConstantTripCount(induction, INT32_MAX) > 0;
    if ((!memory_effects_->IsSafeToSpeculateLoad(load->GetPtr()) && !guaranteed_execution) ||
        memory_effects_->LoopMayWrite(loop, load->GetPtr()) ||
        !CanHoistInvariantCondition(loop, load->GetPtr(), visiting, conditionally_executed)) {
      visiting->erase(value);
      return false;
    }
    visiting->erase(value);
    return true;
  }
  if (mlir::DynamicOrNull<mlir::var::VarGetPtrOp>(value->op_) == nullptr && !IsPureInvariantOperation(value->op_)) {
    visiting->erase(value);
    return false;
  }
  for (auto *operand : value->op_->operands_) {
    if (!CanHoistInvariantCondition(loop, operand, visiting, conditionally_executed)) {
      visiting->erase(value);
      return false;
    }
  }
  visiting->erase(value);
  return true;
}

auto MLIRLoopUnswitchPass::HoistInvariantCondition(mlir::scf::WhileOp *loop, mlir::Value *value,
                                                   std::vector<mlir::Operation *> *operations) -> mlir::Value * {
  if (!loopiv::IsDefinedInLoop(value, loop)) {
    return value;
  }
  auto mapped = cloner_.value_map_.find(value);
  if (mapped != cloner_.value_map_.end()) {
    return mapped->second;
  }
  for (auto *operand : value->op_->operands_) {
    cloner_.value_map_[operand] = HoistInvariantCondition(loop, operand, operations);
  }
  operations->emplace_back(cloner_.Clone(value->op_));
  return cloner_.RemapValue(value);
}

auto MLIRLoopUnswitchPass::Unswitch(mlir::func::FuncDefOp *func_op, mlir::Block *parent, size_t loop_index,
                                    mlir::scf::WhileOp *loop, const Candidate &candidate) -> void {
  cloner_.value_map_.clear();
  std::vector<mlir::Operation *> hoisted_operations;
  auto *hoisted_condition = HoistInvariantCondition(loop, candidate.condition, &hoisted_operations);

  cloner_.value_map_.clear();
  auto *true_loop = mlir::DynamicOrNull<mlir::scf::WhileOp>(cloner_.Clone(loop));
  auto *true_specialized_condition = cloner_.RemapValue(candidate.condition);
  cloner_.value_map_.clear();
  auto *false_loop = mlir::DynamicOrNull<mlir::scf::WhileOp>(cloner_.Clone(loop));
  auto *false_specialized_condition = cloner_.RemapValue(candidate.condition);
  if (true_loop == nullptr || false_loop == nullptr) {
    throw std::logic_error("LoopUnswitch failed to clone a while operation");
  }

  auto make_region = [&](mlir::scf::WhileOp *cloned_loop) -> mlir::Region * {
    auto *region = new mlir::Region();
    auto *block = new mlir::Block();
    block->operations_.emplace_back(cloned_loop);
    std::vector<mlir::Value *> results(cloned_loop->results_.begin(), cloned_loop->results_.end());
    block->operations_.emplace_back(
        GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::YieldOp>(GET_MLIR_CONTEXT(), results));
    region->blocks_.emplace_back(block);
    return region;
  };

  auto *outer_if = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::IfOp>(
      GET_MLIR_CONTEXT(), loop->GetResultsSize(), hoisted_condition, make_region(true_loop), make_region(false_loop));
  for (size_t index = 0; index < loop->GetResultsSize(); ++index) {
    outer_if->GetResultByIndex(index)->SetType(loop->GetResultByIndex(index)->GetType());
    loop->GetResultByIndex(index)->ReplaceAllUsesWith(outer_if->GetResultByIndex(index));
  }
  parent->operations_.erase(parent->operations_.begin() + static_cast<std::ptrdiff_t>(loop_index));
  auto insertion = parent->operations_.begin() + static_cast<std::ptrdiff_t>(loop_index);
  parent->operations_.insert(insertion, hoisted_operations.begin(), hoisted_operations.end());
  parent->operations_.insert(
      parent->operations_.begin() + static_cast<std::ptrdiff_t>(loop_index + hoisted_operations.size()), outer_if);

  // Register cloned uses before replacing the selected if results.
  mlir::utils::RebuildFunctionUses(func_op);
  if (candidate.partial) {
    auto *true_integer =
        GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::ConstOp>(GET_MLIR_CONTEXT(), static_cast<int>(true));
    auto *false_integer =
        GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::ConstOp>(GET_MLIR_CONTEXT(), static_cast<int>(false));
    true_integer->GetRes()->SetType(GET_TYPE_CONTEXT()->GetIntegerType());
    false_integer->GetRes()->SetType(GET_TYPE_CONTEXT()->GetIntegerType());
    auto *true_constant = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::CastOp>(
        GET_MLIR_CONTEXT(), true_integer->GetRes(), GET_TYPE_CONTEXT()->GetIntegerType(),
        candidate.condition->GetType());
    auto *false_constant = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::CastOp>(
        GET_MLIR_CONTEXT(), false_integer->GetRes(), GET_TYPE_CONTEXT()->GetIntegerType(),
        candidate.condition->GetType());
    true_constant->GetRes()->SetType(candidate.condition->GetType());
    false_constant->GetRes()->SetType(candidate.condition->GetType());
    auto outer_position =
        parent->operations_.begin() + static_cast<std::ptrdiff_t>(loop_index + hoisted_operations.size());
    parent->operations_.insert(outer_position, true_integer);
    parent->operations_.insert(
        parent->operations_.begin() + static_cast<std::ptrdiff_t>(loop_index + hoisted_operations.size() + 1),
        true_constant);
    parent->operations_.insert(
        parent->operations_.begin() + static_cast<std::ptrdiff_t>(loop_index + hoisted_operations.size() + 2),
        false_integer);
    parent->operations_.insert(
        parent->operations_.begin() + static_cast<std::ptrdiff_t>(loop_index + hoisted_operations.size() + 3),
        false_constant);
    true_specialized_condition->ReplaceAllUsesWith(true_constant->GetRes());
    false_specialized_condition->ReplaceAllUsesWith(false_constant->GetRes());
  } else {
    Specialize(true_loop, candidate.body_index, true);
    Specialize(false_loop, candidate.body_index, false);
  }
}

auto MLIRLoopUnswitchPass::Specialize(mlir::scf::WhileOp *loop, size_t body_index, bool take_then) -> void {
  auto *body = loop->GetBodyRegion()->blocks_[0];
  if (body_index >= body->operations_.size()) {
    throw std::logic_error("LoopUnswitch clone changed body shape");
  }
  auto *if_op = mlir::DynamicOrNull<mlir::scf::IfOp>(body->operations_[body_index]);
  if (if_op == nullptr) {
    throw std::logic_error("LoopUnswitch clone lost its candidate if");
  }
  auto *selected_region = take_then ? if_op->GetThenRegion() : if_op->GetElseRegion();
  auto *selected_block = selected_region->blocks_[0];
  auto *yield = mlir::DynamicOrNull<mlir::scf::YieldOp>(selected_block->operations_.back());
  for (size_t index = 0; index < if_op->results_.size(); ++index) {
    if_op->GetResultByIndex(index)->ReplaceAllUsesWith(yield->GetOperands()[index]);
  }

  std::vector<mlir::Operation *> specialized;
  specialized.reserve(body->operations_.size() + selected_block->operations_.size());
  specialized.insert(specialized.end(), body->operations_.begin(),
                     body->operations_.begin() + static_cast<std::ptrdiff_t>(body_index));
  specialized.insert(specialized.end(), selected_block->operations_.begin(), selected_block->operations_.end() - 1);
  specialized.insert(specialized.end(), body->operations_.begin() + static_cast<std::ptrdiff_t>(body_index + 1),
                     body->operations_.end());
  body->operations_ = std::move(specialized);
}

}  // namespace middleend::pass::transform
