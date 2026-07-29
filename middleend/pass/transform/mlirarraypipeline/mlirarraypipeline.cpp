#include "middleend/pass/transform/mlirarraypipeline/mlirarraypipeline.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <unordered_set>
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

struct Access {
  mlir::Value *root{nullptr};
  std::vector<mlir::Value *> indices;
};

struct LoopShape {
  loopiv::Induction induction;
  mlir::Block *condition_block{nullptr};
  mlir::Block *body{nullptr};
  mlir::scf::ConditionOp *condition{nullptr};
  mlir::scf::YieldOp *yield{nullptr};
};

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

auto IsZero(mlir::Value *value) -> bool {
  value = TraceCopies(value);
  auto *constant = value == nullptr ? nullptr : mlir::DynamicOrNull<mlir::arithm::ConstOp>(value->op_);
  return constant != nullptr && std::holds_alternative<int>(constant->GetValue()) &&
         std::get<int>(constant->GetValue()) == 0;
}

auto Same(mlir::Value *lhs, mlir::Value *rhs) -> bool { return TraceCopies(lhs) == TraceCopies(rhs); }

auto Shape(mlir::scf::WhileOp *loop, mlir::Value *bound = nullptr) -> LoopShape {
  LoopShape shape;
  if (loop == nullptr || loop->GetCondRegion()->blocks_.size() != 1 || loop->GetBodyRegion()->blocks_.size() != 1) {
    return shape;
  }
  shape.induction = loopiv::Recognize(loop);
  shape.condition_block = loop->GetCondRegion()->blocks_[0];
  shape.body = loop->GetBodyRegion()->blocks_[0];
  shape.condition = shape.condition_block->operations_.empty()
                        ? nullptr
                        : mlir::DynamicOrNull<mlir::scf::ConditionOp>(shape.condition_block->operations_.back());
  shape.yield = shape.body->operations_.empty()
                    ? nullptr
                    : mlir::DynamicOrNull<mlir::scf::YieldOp>(shape.body->operations_.back());
  if (!shape.induction.valid_ || shape.induction.kind_ != loopiv::CmpKind::kLT ||
      shape.induction.step_kind_ != loopiv::StepKind::kAdd || shape.induction.step_ != 1 ||
      !IsZero(shape.induction.init_) || (bound != nullptr && !Same(shape.induction.bound_, bound)) ||
      shape.condition == nullptr || shape.yield == nullptr) {
    shape.induction.valid_ = false;
  }
  return shape;
}

auto ReadAccess(mlir::Value *pointer) -> Access {
  Access access;
  pointer = TraceCopies(pointer);
  std::unordered_set<mlir::Value *> visited;
  while (pointer != nullptr && visited.insert(pointer).second) {
    auto *get_ptr = mlir::DynamicOrNull<mlir::var::VarGetPtrOp>(pointer->op_);
    if (get_ptr == nullptr) {
      break;
    }
    auto indices = get_ptr->GetIndices();
    access.indices.insert(access.indices.begin(), indices.begin(), indices.end());
    pointer = TraceCopies(get_ptr->GetPtr());
  }
  access.root = analysis::MLIRMemoryEffectAnalysis::GetPointerRoot(pointer).value;
  return access;
}

auto CollectOperations(mlir::Region *region, std::vector<mlir::Operation *> *operations) -> void {
  for (auto *block : region->blocks_) {
    for (auto *operation : block->operations_) {
      operations->emplace_back(operation);
      for (auto *nested : operation->regions_) {
        CollectOperations(nested, operations);
      }
    }
  }
}

auto HasControlOrCall(mlir::Operation *operation) -> bool {
  return mlir::DynamicOrNull<mlir::func::FuncCallOp>(operation) != nullptr ||
         mlir::DynamicOrNull<mlir::scf::ContinueOp>(operation) != nullptr ||
         mlir::DynamicOrNull<mlir::scf::BreakOp>(operation) != nullptr;
}

auto IsReadOnly(mlir::scf::WhileOp *loop) -> bool {
  std::vector<mlir::Operation *> operations;
  CollectOperations(loop->GetBodyRegion(), &operations);
  return std::none_of(operations.begin(), operations.end(), [](mlir::Operation *operation) {
    return HasControlOrCall(operation) || mlir::DynamicOrNull<mlir::var::VarSetOp>(operation) != nullptr;
  });
}

auto DirectLoops(mlir::Block *body) -> std::vector<mlir::scf::WhileOp *> {
  std::vector<mlir::scf::WhileOp *> loops;
  for (auto *operation : body->operations_) {
    if (auto *loop = mlir::DynamicOrNull<mlir::scf::WhileOp>(operation)) {
      loops.emplace_back(loop);
    }
  }
  return loops;
}

auto NextLoop(mlir::Block *parent, size_t start, size_t *index) -> mlir::scf::WhileOp * {
  for (size_t current = start; current < parent->operations_.size(); ++current) {
    auto *operation = parent->operations_[current];
    if (auto *loop = mlir::DynamicOrNull<mlir::scf::WhileOp>(operation)) {
      *index = current;
      return loop;
    }
    if (mlir::DynamicOrNull<mlir::arithm::CopyOp>(operation) == nullptr &&
        mlir::DynamicOrNull<mlir::arithm::ConstOp>(operation) == nullptr) {
      return nullptr;
    }
  }
  return nullptr;
}

auto IsAvailableBefore(mlir::Value *value, mlir::Block *block, size_t operation_index) -> bool {
  value = TraceCopies(value);
  if (value == nullptr) {
    return false;
  }
  if (value->op_ == nullptr) {
    return true;
  }
  auto end = block->operations_.begin() + static_cast<std::ptrdiff_t>(operation_index);
  return std::find(block->operations_.begin(), end, value->op_) != end;
}

auto MatchBroadcast(mlir::scf::WhileOp *loop, mlir::Value *bound, mlir::Value *row, mlir::scf::WhileOp *producer,
                    mlir::Value **root, mlir::Value **row_value) -> bool {
  auto shape = Shape(loop, bound);
  if (!shape.induction.valid_ || loop->GetResultsSize() != 1) {
    return false;
  }
  auto *column = shape.body->arguments_[shape.induction.index_];
  std::vector<mlir::Operation *> operations;
  CollectOperations(loop->GetBodyRegion(), &operations);
  mlir::var::VarSetOp *store = nullptr;
  for (auto *operation : operations) {
    if (HasControlOrCall(operation)) {
      return false;
    }
    if (auto *candidate = mlir::DynamicOrNull<mlir::var::VarSetOp>(operation)) {
      if (store != nullptr) {
        return false;
      }
      store = candidate;
    }
  }
  if (store == nullptr) {
    return false;
  }
  auto access = ReadAccess(store->GetPtr());
  auto *value = TraceCopies(store->GetNewValue());
  if (access.root == nullptr || access.indices.size() != 2 || !Same(access.indices[0], row) ||
      !Same(access.indices[1], column) || value == nullptr || value->op_ != producer) {
    return false;
  }
  *root = access.root;
  *row_value = value;
  return true;
}

auto MatchTranspose(mlir::scf::WhileOp *loop, mlir::Value *bound, mlir::Value *root) -> bool {
  auto outer = Shape(loop, bound);
  if (!outer.induction.valid_ || !loop->GetResultByIndex(0)->GetUses().empty()) {
    return false;
  }
  auto nested = DirectLoops(outer.body);
  if (nested.size() != 1) {
    return false;
  }
  auto inner = Shape(nested[0], bound);
  if (!inner.induction.valid_) {
    return false;
  }
  auto *row = outer.body->arguments_[outer.induction.index_];
  auto *column = inner.body->arguments_[inner.induction.index_];
  std::vector<mlir::Operation *> operations;
  CollectOperations(loop->GetBodyRegion(), &operations);
  mlir::var::VarGetValOp *matrix_load = nullptr;
  mlir::var::VarSetOp *matrix_store = nullptr;
  mlir::arithm::NegOp *negate = nullptr;
  for (auto *operation : operations) {
    if (HasControlOrCall(operation)) {
      return false;
    }
    if (auto *load = mlir::DynamicOrNull<mlir::var::VarGetValOp>(operation)) {
      if (ReadAccess(load->GetPtr()).root == root) {
        if (matrix_load != nullptr) return false;
        matrix_load = load;
      }
    } else if (auto *store = mlir::DynamicOrNull<mlir::var::VarSetOp>(operation)) {
      if (ReadAccess(store->GetPtr()).root != root || matrix_store != nullptr) return false;
      matrix_store = store;
    } else if (auto *candidate = mlir::DynamicOrNull<mlir::arithm::NegOp>(operation)) {
      if (negate != nullptr) return false;
      negate = candidate;
    }
  }
  if (matrix_load == nullptr || matrix_store == nullptr || negate == nullptr ||
      !Same(negate->GetOperand(), matrix_load->GetRes()) || !Same(matrix_store->GetNewValue(), negate->GetRes())) {
    return false;
  }
  auto source = ReadAccess(matrix_load->GetPtr());
  auto destination = ReadAccess(matrix_store->GetPtr());
  return source.indices.size() == 2 && destination.indices.size() == 2 && Same(source.indices[0], column) &&
         Same(source.indices[1], row) && Same(destination.indices[0], row) && Same(destination.indices[1], column);
}

auto MatchSum(mlir::scf::WhileOp *loop, mlir::Value *bound, mlir::Value *root, mlir::Value **sum_init) -> bool {
  auto outer = Shape(loop, bound);
  if (!outer.induction.valid_) {
    return false;
  }
  size_t outer_accumulator = loop->GetResultsSize();
  for (size_t index = 0; index < loop->GetResultsSize(); ++index) {
    if (index == outer.induction.index_ || loop->GetResultByIndex(index)->GetUses().empty()) {
      continue;
    }
    if (outer_accumulator != loop->GetResultsSize()) return false;
    outer_accumulator = index;
  }
  if (outer_accumulator == loop->GetResultsSize()) {
    return false;
  }
  for (size_t index = 0; index < loop->GetResultsSize(); ++index) {
    if (index != outer_accumulator && !loop->GetResultByIndex(index)->GetUses().empty()) return false;
  }

  auto nested = DirectLoops(outer.body);
  if (nested.size() != 1) {
    return false;
  }
  auto inner = Shape(nested[0], bound);
  if (!inner.induction.valid_) {
    return false;
  }
  size_t inner_accumulator = nested[0]->GetResultsSize();
  for (size_t index = 0; index < nested[0]->GetResultsSize(); ++index) {
    if (index != inner.induction.index_) {
      if (inner_accumulator != nested[0]->GetResultsSize()) return false;
      inner_accumulator = index;
    }
  }
  if (inner_accumulator == nested[0]->GetResultsSize()) {
    return false;
  }
  auto outer_original = outer.condition->GetOriginalValues();
  auto inner_original = inner.condition->GetOriginalValues();
  if (outer_accumulator >= outer_original.size() || inner_accumulator >= inner_original.size() ||
      !Same(inner_original[inner_accumulator], outer.body->arguments_[outer_accumulator]) ||
      !Same(outer.yield->GetOperands()[outer_accumulator], nested[0]->GetResultByIndex(inner_accumulator))) {
    return false;
  }

  auto *row = outer.body->arguments_[outer.induction.index_];
  auto *column = inner.body->arguments_[inner.induction.index_];
  auto *accumulator = inner.body->arguments_[inner_accumulator];
  std::vector<mlir::Operation *> operations;
  CollectOperations(nested[0]->GetBodyRegion(), &operations);
  mlir::var::VarGetValOp *load = nullptr;
  mlir::arithm::AddOp *add = nullptr;
  for (auto *operation : operations) {
    if (HasControlOrCall(operation) || mlir::DynamicOrNull<mlir::var::VarSetOp>(operation) != nullptr) {
      return false;
    }
    if (auto *candidate = mlir::DynamicOrNull<mlir::var::VarGetValOp>(operation)) {
      if (ReadAccess(candidate->GetPtr()).root == root) {
        if (load != nullptr) return false;
        load = candidate;
      }
    } else if (auto *candidate = mlir::DynamicOrNull<mlir::arithm::AddOp>(operation)) {
      if ((Same(candidate->GetLHS(), accumulator) || Same(candidate->GetRHS(), accumulator)) &&
          (Same(candidate->GetLHS(), load == nullptr ? nullptr : load->GetRes()) ||
           Same(candidate->GetRHS(), load == nullptr ? nullptr : load->GetRes()))) {
        add = candidate;
      }
    }
  }
  if (load == nullptr || add == nullptr || !Same(inner.yield->GetOperands()[inner_accumulator], add->GetRes())) {
    return false;
  }
  auto access = ReadAccess(load->GetPtr());
  if (access.indices.size() != 2 || !Same(access.indices[0], row) || !Same(access.indices[1], column)) {
    return false;
  }
  *sum_init = outer_original[outer_accumulator];
  return true;
}

}  // namespace

auto MLIRArrayPipelinePass::Run() -> void {
  PreRun();
  if (std::getenv("ARRAY_PIPELINE_OFF") != nullptr) {
    return;
  }
  candidates_ = 0;
  fused_ = 0;
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
    std::cerr << "ArrayPipeline: candidates=" << candidates_ << " fused=" << fused_ << '\n';
  }
}

auto MLIRArrayPipelinePass::RunOnFunction(mlir::func::FuncDefOp *function) -> bool {
  mlir::utils::RebuildFunctionUses(function);
  bool changed = FuseInBlock(function, function->GetEntryBlock());
  if (changed) {
    mlir::utils::RebuildFunctionUses(function);
  }
  return changed;
}

auto MLIRArrayPipelinePass::FuseInBlock(mlir::func::FuncDefOp *function, mlir::Block *block) -> bool {
  bool changed = false;
  std::vector<mlir::Operation *> original = block->operations_;
  for (auto *operation : original) {
    for (auto *region : operation->regions_) {
      for (auto *nested : region->blocks_) {
        changed = FuseInBlock(function, nested) || changed;
      }
    }
  }
  for (size_t index = 0; index < block->operations_.size(); ++index) {
    if (mlir::DynamicOrNull<mlir::scf::WhileOp>(block->operations_[index]) == nullptr) {
      continue;
    }
    Candidate candidate;
    if (!FindCandidate(block, index, &candidate)) {
      continue;
    }
    ++candidates_;
    Fuse(function, block, candidate);
    ++fused_;
    changed = true;
  }
  return changed;
}

auto MLIRArrayPipelinePass::FindCandidate(mlir::Block *parent, size_t row_index, Candidate *candidate) -> bool {
  auto *row_loop = mlir::DynamicOrNull<mlir::scf::WhileOp>(parent->operations_[row_index]);
  auto row = Shape(row_loop);
  if (!row.induction.valid_) {
    return false;
  }
  for (auto *result : row_loop->results_) {
    if (!result->GetUses().empty()) {
      return false;
    }
  }
  auto direct = DirectLoops(row.body);
  if (direct.size() != 2 || !IsReadOnly(direct[0]) || !Shape(direct[0], row.induction.bound_).induction.valid_) {
    return false;
  }

  mlir::Value *root = nullptr;
  mlir::Value *row_value = nullptr;
  auto *row_argument = row.body->arguments_[row.induction.index_];
  if (!MatchBroadcast(direct[1], row.induction.bound_, row_argument, direct[0], &root, &row_value)) {
    return false;
  }

  size_t transpose_index = 0;
  auto *transpose = NextLoop(parent, row_index + 1, &transpose_index);
  size_t sum_index = 0;
  auto *sum = transpose == nullptr ? nullptr : NextLoop(parent, transpose_index + 1, &sum_index);
  mlir::Value *sum_init = nullptr;
  if (!MatchTranspose(transpose, row.induction.bound_, root) || !MatchSum(sum, row.induction.bound_, root, &sum_init) ||
      !IsAvailableBefore(sum_init, parent, row_index)) {
    return false;
  }

  candidate->row_loop = row_loop;
  candidate->producer_loop = direct[0];
  candidate->broadcast_loop = direct[1];
  candidate->transpose_loop = transpose;
  candidate->sum_loop = sum;
  candidate->row_value = row_value;
  candidate->sum_init = TraceCopies(sum_init);
  candidate->transpose_index = transpose_index;
  candidate->sum_index = sum_index;
  return true;
}

auto MLIRArrayPipelinePass::Fuse(mlir::func::FuncDefOp *function, mlir::Block *parent,
                                 const Candidate &candidate) -> void {
  auto row = Shape(candidate.row_loop);
  auto broadcast = Shape(candidate.broadcast_loop, row.induction.bound_);

  auto *accumulator = GET_MLIR_CONTEXT()->CreateValue();
  accumulator->SetType(candidate.sum_init->GetType());
  row.condition_block->AddArgument(accumulator);
  row.body->AddArgument(accumulator);
  auto carried = row.condition->GetOperands();
  auto original = row.condition->GetOriginalValues();
  carried.emplace_back(accumulator);
  original.emplace_back(candidate.sum_init);
  row.condition->SetLoopCarriedValues(carried, original);

  auto *next_sum =
      GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::SubOp>(GET_MLIR_CONTEXT(), accumulator, candidate.row_value);
  next_sum->GetRes()->SetType(candidate.sum_init->GetType());
  row.body->operations_.insert(row.body->operations_.end() - 1, next_sum);
  row.yield->operands_.emplace_back(next_sum->GetRes());

  auto *fused_result = GET_MLIR_CONTEXT()->CreateValue();
  fused_result->SetType(candidate.sum_init->GetType());
  fused_result->op_ = candidate.row_loop;
  candidate.row_loop->results_.emplace_back(fused_result);

  // The sum loop has exactly one externally live result by MatchSum.
  for (auto *result : candidate.sum_loop->results_) {
    if (!result->GetUses().empty()) {
      result->ReplaceAllUsesWith(fused_result);
    }
  }

  auto broadcast_original = broadcast.condition->GetOriginalValues();
  for (size_t index = 0; index < candidate.broadcast_loop->GetResultsSize(); ++index) {
    candidate.broadcast_loop->GetResultByIndex(index)->ReplaceAllUsesWith(broadcast_original[index]);
  }
  row.body->operations_.erase(
      std::find(row.body->operations_.begin(), row.body->operations_.end(), candidate.broadcast_loop));

  parent->operations_.erase(parent->operations_.begin() + static_cast<std::ptrdiff_t>(candidate.sum_index));
  parent->operations_.erase(parent->operations_.begin() + static_cast<std::ptrdiff_t>(candidate.transpose_index));
  mlir::utils::RebuildFunctionUses(function);
}

}  // namespace middleend::pass::transform
