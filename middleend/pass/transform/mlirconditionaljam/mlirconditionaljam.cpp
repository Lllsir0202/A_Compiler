#include "middleend/pass/transform/mlirconditionaljam/mlirconditionaljam.h"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <unordered_map>
#include <vector>
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/arithm/binaryop.h"
#include "middleend/mlir/dialect/arithm/copy.h"
#include "middleend/mlir/dialect/func/funccall.h"
#include "middleend/mlir/dialect/func/funcdef.h"
#include "middleend/mlir/dialect/scf/condition.h"
#include "middleend/mlir/dialect/scf/if.h"
#include "middleend/mlir/dialect/scf/while.h"
#include "middleend/mlir/dialect/scf/yield.h"
#include "middleend/mlir/dialect/var/getptr.h"
#include "middleend/mlir/dialect/var/getvalue.h"
#include "middleend/mlir/dialect/var/set.h"
#include "middleend/mlir/utils/clone/uselistrebuilder.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/block.h"
#include "middleend/mlir/utils/struct/region.h"
#include "middleend/mlir/utils/value/value.h"
#include "middleend/pass/analysis/mlirmemoryeffects/mlirmemoryeffects.h"
#include "middleend/pass/transform/mlirloopunroll/inductionvariable.h"

#define GET_MLIR_CONTEXT() compiler_context_->GetIRContext()->GetMLIRContext()

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

auto Same(mlir::Value *lhs, mlir::Value *rhs) -> bool { return TraceCopies(lhs) == TraceCopies(rhs); }

auto ContainsIf(mlir::Region *region) -> bool {
  for (auto *block : region->blocks_) {
    for (auto *operation : block->operations_) {
      if (mlir::DynamicOrNull<mlir::scf::IfOp>(operation) != nullptr) {
        return true;
      }
      for (auto *nested : operation->regions_) {
        if (ContainsIf(nested)) {
          return true;
        }
      }
    }
  }
  return false;
}

auto Root(mlir::Value *pointer) -> mlir::Value * {
  return analysis::MLIRMemoryEffectAnalysis::GetPointerRoot(pointer).value;
}

auto CollectLoads(mlir::Region *region, std::vector<mlir::var::VarGetValOp *> *loads) -> bool {
  for (auto *block : region->blocks_) {
    for (auto *operation : block->operations_) {
      if (auto *load = mlir::DynamicOrNull<mlir::var::VarGetValOp>(operation)) {
        loads->emplace_back(load);
      }
      if (mlir::DynamicOrNull<mlir::var::VarSetOp>(operation) != nullptr ||
          mlir::DynamicOrNull<mlir::func::FuncCallOp>(operation) != nullptr ||
          mlir::DynamicOrNull<mlir::scf::WhileOp>(operation) != nullptr) {
        return false;
      }
      for (auto *nested : operation->regions_) {
        if (!CollectLoads(nested, loads)) {
          return false;
        }
      }
    }
  }
  return true;
}

}  // namespace

auto MLIRConditionalJamPass::Run() -> void {
  PreRun();
  if (std::getenv("CONDITIONAL_JAM_OFF") != nullptr) {
    return;
  }
  candidates_ = 0;
  jammed_ = 0;
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
    std::cerr << "ConditionalJam: candidates=" << candidates_ << " jammed=" << jammed_ << '\n';
  }
}

auto MLIRConditionalJamPass::RunOnFunction(mlir::func::FuncDefOp *function) -> bool {
  mlir::utils::RebuildFunctionUses(function);
  bool changed = JamInBlock(function->GetEntryBlock());
  if (changed) {
    mlir::utils::RebuildFunctionUses(function);
  }
  return changed;
}

auto MLIRConditionalJamPass::JamInBlock(mlir::Block *block) -> bool {
  bool changed = false;
  std::vector<mlir::Operation *> original = block->operations_;
  for (auto *operation : original) {
    for (auto *region : operation->regions_) {
      for (auto *nested : region->blocks_) {
        changed = JamInBlock(nested) || changed;
      }
    }
  }
  for (auto *operation : block->operations_) {
    auto *loop = mlir::DynamicOrNull<mlir::scf::WhileOp>(operation);
    if (loop == nullptr) {
      continue;
    }
    Candidate candidate;
    if (!FindCandidate(loop, &candidate)) {
      continue;
    }
    ++candidates_;
    Jam(candidate);
    ++jammed_;
    changed = true;
  }
  return changed;
}

auto MLIRConditionalJamPass::FindCandidate(mlir::scf::WhileOp *loop, Candidate *candidate) -> bool {
  auto column = loopiv::Recognize(loop);
  int64_t trip_count = loopiv::ConstantTripCount(column, INT32_MAX);
  if (!column.valid_ || column.kind_ != loopiv::CmpKind::kLT || column.step_kind_ != loopiv::StepKind::kAdd ||
      column.step_ != 1 || trip_count < kLanes || trip_count % kLanes != 0 ||
      loop->GetCondRegion()->blocks_.size() != 1 || loop->GetBodyRegion()->blocks_.size() != 1) {
    return false;
  }
  auto *column_body = loop->GetBodyRegion()->blocks_[0];
  auto *column_yield = column_body->operations_.empty()
                           ? nullptr
                           : mlir::DynamicOrNull<mlir::scf::YieldOp>(column_body->operations_.back());
  if (column_yield == nullptr) {
    return false;
  }

  mlir::scf::WhileOp *reduction_loop = nullptr;
  mlir::var::VarSetOp *output_store = nullptr;
  for (auto *operation : column_body->operations_) {
    if (auto *nested = mlir::DynamicOrNull<mlir::scf::WhileOp>(operation)) {
      if (reduction_loop != nullptr) return false;
      reduction_loop = nested;
    } else if (auto *store = mlir::DynamicOrNull<mlir::var::VarSetOp>(operation)) {
      if (output_store != nullptr) return false;
      output_store = store;
    } else if (mlir::DynamicOrNull<mlir::func::FuncCallOp>(operation) != nullptr) {
      return false;
    }
  }
  if (reduction_loop == nullptr || output_store == nullptr || !ContainsIf(reduction_loop->GetBodyRegion()) ||
      !loopiv::BodyIsCloneable(reduction_loop->GetBodyRegion()) || reduction_loop->GetResultsSize() != 2) {
    return false;
  }
  auto reduction = loopiv::Recognize(reduction_loop);
  if (!reduction.valid_ || reduction.kind_ != loopiv::CmpKind::kLT || reduction.step_kind_ != loopiv::StepKind::kAdd ||
      reduction.step_ != 1 || loopiv::IsDefinedInLoop(reduction.bound_, reduction_loop)) {
    return false;
  }
  size_t accumulator_index = 1 - reduction.index_;
  if (!Same(output_store->GetNewValue(), reduction_loop->GetResultByIndex(accumulator_index))) {
    return false;
  }
  auto *traced_pointer = TraceCopies(output_store->GetPtr());
  auto *output_pointer =
      traced_pointer == nullptr ? nullptr : mlir::DynamicOrNull<mlir::var::VarGetPtrOp>(traced_pointer->op_);
  if (output_pointer == nullptr || output_pointer->GetIndices().empty()) {
    return false;
  }
  size_t column_indices = 0;
  auto *column_argument = column_body->arguments_[column.index_];
  for (auto *index : output_pointer->GetIndices()) {
    column_indices += Same(index, column_argument) ? 1 : 0;
  }
  if (column_indices != 1) {
    return false;
  }

  std::vector<mlir::var::VarGetValOp *> loads;
  if (!CollectLoads(reduction_loop->GetBodyRegion(), &loads)) {
    return false;
  }
  auto *output_root = Root(output_pointer->GetRes());
  if (output_root == nullptr || std::any_of(loads.begin(), loads.end(), [&](mlir::var::VarGetValOp *load) {
        return Root(load->GetPtr()) == output_root;
      })) {
    return false;
  }
  auto reduction_original =
      mlir::DynamicOrNull<mlir::scf::ConditionOp>(reduction_loop->GetCondRegion()->blocks_[0]->operations_.back())
          ->GetOriginalValues();
  if (reduction_original.size() != 2) {
    return false;
  }
  auto *reduction_init = TraceCopies(reduction_original[reduction.index_]);
  auto *accumulator_init = TraceCopies(reduction_original[accumulator_index]);
  if (reduction_init == nullptr || accumulator_init == nullptr || loopiv::IsDefinedInLoop(reduction_init, loop) ||
      loopiv::IsDefinedInLoop(accumulator_init, loop)) {
    return false;
  }

  // Every non-column loop result must be exactly one of the reduction results;
  // this lets the jammed loop preserve lexical loop-carried state.
  for (size_t index = 0; index < column_yield->GetOperands().size(); ++index) {
    if (index == column.index_) continue;
    bool mapped = false;
    for (size_t reduction_result = 0; reduction_result < 2; ++reduction_result) {
      mapped = mapped || Same(column_yield->GetOperands()[index], reduction_loop->GetResultByIndex(reduction_result));
    }
    if (!mapped) return false;
  }

  auto *next_column = TraceCopies(column_yield->GetOperands()[column.index_]);
  auto *column_add = next_column == nullptr ? nullptr : mlir::DynamicOrNull<mlir::arithm::AddOp>(next_column->op_);
  if (column_add == nullptr) {
    return false;
  }
  mlir::Value *step =
      TraceCopies(Same(column_add->GetLHS(), column_argument) ? column_add->GetRHS() : column_add->GetLHS());
  if (!Same(column_add->GetLHS(), column_argument) && !Same(column_add->GetRHS(), column_argument)) {
    return false;
  }
  if (step == nullptr || loopiv::IsDefinedInLoop(step, loop) ||
      loopiv::IsDefinedInLoop(output_pointer->GetPtr(), loop)) {
    return false;
  }

  candidate->column_loop = loop;
  candidate->reduction_loop = reduction_loop;
  candidate->output_pointer = output_pointer;
  candidate->output_store = output_store;
  candidate->column_index = column.index_;
  candidate->reduction_index = reduction.index_;
  candidate->accumulator_index = accumulator_index;
  candidate->column_step = step;
  candidate->reduction_init = reduction_init;
  candidate->accumulator_init = accumulator_init;
  return true;
}

auto MLIRConditionalJamPass::Jam(const Candidate &candidate) -> void {
  auto *column_body = candidate.column_loop->GetBodyRegion()->blocks_[0];
  auto *column_argument = column_body->arguments_[candidate.column_index];
  auto *column_yield = mlir::DynamicOrNull<mlir::scf::YieldOp>(column_body->operations_.back());
  auto *reduction_cond = candidate.reduction_loop->GetCondRegion()->blocks_[0];
  auto *reduction_body = candidate.reduction_loop->GetBodyRegion()->blocks_[0];
  auto *reduction_condition = mlir::DynamicOrNull<mlir::scf::ConditionOp>(reduction_cond->operations_.back());
  auto *reduction_yield = mlir::DynamicOrNull<mlir::scf::YieldOp>(reduction_body->operations_.back());

  std::vector<mlir::Operation *> operations;
  std::vector<mlir::Value *> columns{column_argument};
  for (int64_t lane = 1; lane < kLanes; ++lane) {
    auto *next = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::AddOp>(GET_MLIR_CONTEXT(), columns.back(),
                                                                          candidate.column_step);
    next->GetRes()->SetType(column_argument->GetType());
    operations.emplace_back(next);
    columns.emplace_back(next->GetRes());
  }

  std::vector<mlir::Value *> jam_arguments;
  auto *jam_induction = GET_MLIR_CONTEXT()->CreateValue();
  jam_induction->SetType(reduction_cond->arguments_[candidate.reduction_index]->GetType());
  jam_arguments.emplace_back(jam_induction);
  for (int64_t lane = 0; lane < kLanes; ++lane) {
    auto *accumulator = GET_MLIR_CONTEXT()->CreateValue();
    accumulator->SetType(reduction_cond->arguments_[candidate.accumulator_index]->GetType());
    jam_arguments.emplace_back(accumulator);
  }

  auto *jam_cond_block = new mlir::Block();
  auto *jam_body_block = new mlir::Block();
  for (auto *argument : jam_arguments) {
    jam_cond_block->AddArgument(argument);
    jam_body_block->AddArgument(argument);
  }
  auto *compare = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::LessOp>(
      GET_MLIR_CONTEXT(), jam_induction, loopiv::Recognize(candidate.reduction_loop).bound_);
  compare->GetRes()->SetType(reduction_condition->GetCondition()->GetType());
  jam_cond_block->AddOperation(compare);
  auto *condition = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::ConditionOp>(
      GET_MLIR_CONTEXT(), compare->GetRes(), std::unordered_map<symboltable::VarEntry *, mlir::Value *>{});
  std::vector<mlir::Value *> jam_inits{candidate.reduction_init};
  jam_inits.insert(jam_inits.end(), kLanes, candidate.accumulator_init);
  condition->SetLoopCarriedValues(jam_arguments, jam_inits);
  jam_cond_block->AddOperation(condition);

  std::vector<mlir::Value *> next_accumulators;
  mlir::Value *next_induction = nullptr;
  for (int64_t lane = 0; lane < kLanes; ++lane) {
    cloner_.value_map_.clear();
    cloner_.value_map_[column_argument] = columns[lane];
    cloner_.value_map_[reduction_body->arguments_[candidate.reduction_index]] = jam_induction;
    cloner_.value_map_[reduction_body->arguments_[candidate.accumulator_index]] = jam_arguments[lane + 1];
    for (size_t index = 0; index + 1 < reduction_body->operations_.size(); ++index) {
      jam_body_block->AddOperation(cloner_.Clone(reduction_body->operations_[index]));
    }
    if (lane == 0) {
      next_induction = cloner_.RemapValue(reduction_yield->GetOperands()[candidate.reduction_index]);
    }
    next_accumulators.emplace_back(cloner_.RemapValue(reduction_yield->GetOperands()[candidate.accumulator_index]));
  }
  std::vector<mlir::Value *> jam_yields{next_induction};
  jam_yields.insert(jam_yields.end(), next_accumulators.begin(), next_accumulators.end());
  jam_body_block->AddOperation(GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::YieldOp>(GET_MLIR_CONTEXT(), jam_yields));

  auto *cond_region = new mlir::Region();
  auto *body_region = new mlir::Region();
  cond_region->AddBlock(jam_cond_block);
  body_region->AddBlock(jam_body_block);
  auto *jam_loop =
      GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::WhileOp>(GET_MLIR_CONTEXT(), kLanes + 1, cond_region, body_region);
  jam_loop->GetResultByIndex(0)->SetType(
      candidate.reduction_loop->GetResultByIndex(candidate.reduction_index)->GetType());
  for (int64_t lane = 0; lane < kLanes; ++lane) {
    jam_loop->GetResultByIndex(lane + 1)->SetType(
        candidate.reduction_loop->GetResultByIndex(candidate.accumulator_index)->GetType());
  }
  operations.emplace_back(jam_loop);

  auto original_indices = candidate.output_pointer->GetIndices();
  for (int64_t lane = 0; lane < kLanes; ++lane) {
    auto indices = original_indices;
    for (auto *&index : indices) {
      if (Same(index, column_argument)) {
        index = columns[lane];
      }
    }
    auto *pointer = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarGetPtrOp>(
        GET_MLIR_CONTEXT(), candidate.output_pointer->GetPtr(), indices);
    pointer->GetRes()->SetType(candidate.output_pointer->GetRes()->GetType());
    operations.emplace_back(pointer);
    auto *store = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarSetOp>(GET_MLIR_CONTEXT(), pointer->GetRes(),
                                                                           jam_loop->GetResultByIndex(lane + 1));
    store->GetRes()->SetType(candidate.output_store->GetRes()->GetType());
    operations.emplace_back(store);
  }

  auto *next_column = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::AddOp>(GET_MLIR_CONTEXT(), columns.back(),
                                                                               candidate.column_step);
  next_column->GetRes()->SetType(column_argument->GetType());
  operations.emplace_back(next_column);
  std::vector<mlir::Value *> final_yields;
  for (size_t index = 0; index < column_yield->GetOperands().size(); ++index) {
    if (index == candidate.column_index) {
      final_yields.emplace_back(next_column->GetRes());
    } else if (Same(column_yield->GetOperands()[index],
                    candidate.reduction_loop->GetResultByIndex(candidate.reduction_index))) {
      final_yields.emplace_back(jam_loop->GetResultByIndex(0));
    } else {
      final_yields.emplace_back(jam_loop->GetResultByIndex(kLanes));
    }
  }
  operations.emplace_back(GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::YieldOp>(GET_MLIR_CONTEXT(), final_yields));
  column_body->operations_ = std::move(operations);
}

}  // namespace middleend::pass::transform
