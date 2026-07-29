#include "middleend/pass/transform/mlirstencilinterior/mlirstencilinterior.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <unordered_set>
#include <vector>
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/arithm/binaryop.h"
#include "middleend/mlir/dialect/arithm/copy.h"
#include "middleend/mlir/dialect/arithm/unaryop.h"
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

auto Same(mlir::Value *lhs, mlir::Value *rhs) -> bool {
  return TraceCopies(lhs) == TraceCopies(rhs);
}

auto ConstantInt(mlir::Value *value, int64_t *result,
                 std::unordered_set<mlir::Value *> *visited) -> bool {
  value = TraceCopies(value);
  if (value == nullptr || !visited->insert(value).second) {
    return false;
  }
  if (auto *constant = mlir::DynamicOrNull<mlir::arithm::ConstOp>(value->op_)) {
    if (!std::holds_alternative<int>(constant->GetValue())) {
      return false;
    }
    *result = std::get<int>(constant->GetValue());
    return true;
  }
  auto evaluate = [&](auto *operation, auto combine) {
    if (operation == nullptr) {
      return false;
    }
    int64_t lhs = 0;
    int64_t rhs = 0;
    if (!ConstantInt(operation->GetLHS(), &lhs, visited) ||
        !ConstantInt(operation->GetRHS(), &rhs, visited)) {
      return false;
    }
    return combine(lhs, rhs);
  };
  if (auto *add = mlir::DynamicOrNull<mlir::arithm::AddOp>(value->op_)) {
    return evaluate(add, [&](int64_t lhs, int64_t rhs) {
      *result = lhs + rhs;
      return true;
    });
  }
  if (auto *sub = mlir::DynamicOrNull<mlir::arithm::SubOp>(value->op_)) {
    return evaluate(sub, [&](int64_t lhs, int64_t rhs) {
      *result = lhs - rhs;
      return true;
    });
  }
  if (auto *multiply = mlir::DynamicOrNull<mlir::arithm::MulOp>(value->op_)) {
    return evaluate(multiply, [&](int64_t lhs, int64_t rhs) {
      *result = lhs * rhs;
      return true;
    });
  }
  if (auto *divide = mlir::DynamicOrNull<mlir::arithm::DivOp>(value->op_)) {
    return evaluate(divide, [&](int64_t lhs, int64_t rhs) {
      if (rhs == 0 || (lhs == std::numeric_limits<int32_t>::min() && rhs == -1)) {
        return false;
      }
      *result = lhs / rhs;
      return true;
    });
  }
  return false;
}

auto ConstantInt(mlir::Value *value, int64_t *result) -> bool {
  std::unordered_set<mlir::Value *> visited;
  return ConstantInt(value, result, &visited);
}

auto IsZero(mlir::Value *value) -> bool {
  int64_t constant = 0;
  return ConstantInt(value, &constant) && constant == 0;
}

auto DirectLoops(mlir::Block *block) -> std::vector<mlir::scf::WhileOp *> {
  std::vector<mlir::scf::WhileOp *> loops;
  for (auto *operation : block->operations_) {
    if (auto *loop = mlir::DynamicOrNull<mlir::scf::WhileOp>(operation)) {
      loops.emplace_back(loop);
    }
  }
  return loops;
}

auto RegionYield(mlir::Region *region) -> mlir::scf::YieldOp * {
  if (region == nullptr || region->blocks_.size() != 1 ||
      region->blocks_[0]->operations_.empty()) {
    return nullptr;
  }
  return mlir::DynamicOrNull<mlir::scf::YieldOp>(
      region->blocks_[0]->operations_.back());
}

auto FlattenAnd(mlir::Value *value, std::vector<mlir::Value *> *leaves,
                std::vector<mlir::scf::IfOp *> *logical_ands) -> bool {
  value = TraceCopies(value);
  auto *select = value == nullptr ? nullptr : mlir::DynamicOrNull<mlir::scf::IfOp>(value->op_);
  if (select == nullptr) {
    leaves->emplace_back(value);
    return value != nullptr;
  }
  if (select->results_.size() != 1) {
    return false;
  }
  auto *then_yield = RegionYield(select->GetThenRegion());
  auto *else_yield = RegionYield(select->GetElseRegion());
  if (then_yield == nullptr || else_yield == nullptr ||
      then_yield->GetOperands().size() != 1 ||
      else_yield->GetOperands().size() != 1 ||
      !Same(else_yield->GetOperands()[0], select->GetCondition())) {
    return false;
  }
  logical_ands->emplace_back(select);
  return FlattenAnd(select->GetCondition(), leaves, logical_ands) &&
         FlattenAnd(then_yield->GetOperands()[0], leaves, logical_ands);
}

auto ScalarLoad(mlir::Value *value) -> mlir::var::VarGetValOp * {
  value = TraceCopies(value);
  auto *load = value == nullptr ? nullptr : mlir::DynamicOrNull<mlir::var::VarGetValOp>(value->op_);
  if (load == nullptr) {
    return nullptr;
  }
  auto *traced_pointer = TraceCopies(load->GetPtr());
  auto *pointer =
      traced_pointer == nullptr
          ? nullptr
          : mlir::DynamicOrNull<mlir::var::VarGetPtrOp>(traced_pointer->op_);
  return pointer != nullptr && pointer->GetIndices().empty() ? load : nullptr;
}

auto ScalarRoot(mlir::Value *value) -> mlir::Value * {
  auto *load = ScalarLoad(value);
  return load == nullptr
             ? nullptr
             : analysis::MLIRMemoryEffectAnalysis::GetPointerRoot(load->GetPtr()).value;
}

auto Coordinate(mlir::Value *value, mlir::Value *outer, mlir::Value *inner,
                int32_t *pad) -> bool {
  value = TraceCopies(value);
  auto *subtract =
      value == nullptr ? nullptr : mlir::DynamicOrNull<mlir::arithm::SubOp>(value->op_);
  if (subtract == nullptr) {
    return false;
  }
  auto *traced_sum = TraceCopies(subtract->GetLHS());
  auto *sum =
      traced_sum == nullptr
          ? nullptr
          : mlir::DynamicOrNull<mlir::arithm::AddOp>(traced_sum->op_);
  int64_t constant = 0;
  if (sum == nullptr ||
      !((Same(sum->GetLHS(), outer) && Same(sum->GetRHS(), inner)) ||
        (Same(sum->GetLHS(), inner) && Same(sum->GetRHS(), outer))) ||
      !ConstantInt(subtract->GetRHS(), &constant) || constant < 0 ||
      constant > std::numeric_limits<int32_t>::max()) {
    return false;
  }
  *pad = static_cast<int32_t>(constant);
  return true;
}

auto NoWritesOrCalls(mlir::Region *region) -> bool {
  for (auto *block : region->blocks_) {
    for (auto *operation : block->operations_) {
      if (mlir::DynamicOrNull<mlir::var::VarSetOp>(operation) != nullptr ||
          mlir::DynamicOrNull<mlir::func::FuncCallOp>(operation) != nullptr) {
        return false;
      }
      for (auto *nested : operation->regions_) {
        if (!NoWritesOrCalls(nested)) {
          return false;
        }
      }
    }
  }
  return true;
}

auto MakeYieldRegion(context::MLIRContext *context, mlir::Operation *operation,
                     mlir::Value *value) -> mlir::Region * {
  auto *block = new mlir::Block();
  if (operation != nullptr) {
    block->AddOperation(operation);
  }
  block->AddOperation(
      context->CreateOperation<mlir::scf::YieldOp>(context, std::vector<mlir::Value *>{value}));
  auto *region = new mlir::Region();
  region->AddBlock(block);
  return region;
}

}  // namespace

auto MLIRStencilInteriorPass::Run() -> void {
  PreRun();
  if (std::getenv("STENCIL_INTERIOR_OFF") != nullptr) {
    return;
  }
  candidates_ = 0;
  specialized_ = 0;
  auto *module = GET_MLIR_CONTEXT()->GetModuleOp();
  if (module == nullptr) {
    throw std::logic_error("Module operation is not set in MLIRContext");
  }
  std::vector<Candidate> candidates;
  for (auto *operation : module->GetBlock()->operations_) {
    auto *function = mlir::DynamicOrNull<mlir::func::FuncDefOp>(operation);
    if (function == nullptr) {
      continue;
    }
    mlir::utils::RebuildFunctionUses(function);
    CollectCandidates(function, function->GetEntryBlock(), nullptr, &candidates);
  }
  candidates_ = candidates.size();
  for (const auto &candidate : candidates) {
    Specialize(candidate);
    mlir::utils::RebuildFunctionUses(candidate.function);
    ++specialized_;
  }
  if (std::getenv("LOOP_OPT_STATS") != nullptr) {
    std::cerr << "StencilInterior: candidates=" << candidates_
              << " specialized=" << specialized_ << '\n';
  }
}

auto MLIRStencilInteriorPass::CollectCandidates(
    mlir::func::FuncDefOp *function, mlir::Block *block,
    mlir::scf::WhileOp *parent, std::vector<Candidate> *candidates) -> void {
  for (auto *operation : block->operations_) {
    auto *loop = mlir::DynamicOrNull<mlir::scf::WhileOp>(operation);
    if (loop != nullptr && parent != nullptr) {
      Candidate candidate;
      if (FindCandidate(function, parent, loop, &candidate)) {
        candidates->emplace_back(std::move(candidate));
      }
    }
    for (auto *region : operation->regions_) {
      for (auto *nested : region->blocks_) {
        CollectCandidates(function, nested, loop != nullptr ? loop : parent, candidates);
      }
    }
  }
}

auto MLIRStencilInteriorPass::FindCandidate(
    mlir::func::FuncDefOp *function, mlir::scf::WhileOp *row_loop,
    mlir::scf::WhileOp *column_loop, Candidate *candidate) -> bool {
  auto row_iv = loopiv::Recognize(row_loop);
  auto column_iv = loopiv::Recognize(column_loop);
  if (!row_iv.valid_ || !column_iv.valid_ ||
      row_iv.kind_ != loopiv::CmpKind::kLT ||
      column_iv.kind_ != loopiv::CmpKind::kLT ||
      row_iv.step_kind_ != loopiv::StepKind::kAdd ||
      column_iv.step_kind_ != loopiv::StepKind::kAdd || row_iv.step_ != 1 ||
      column_iv.step_ != 1 || !IsZero(row_iv.init_) || !IsZero(column_iv.init_) ||
      ScalarRoot(row_iv.bound_) == nullptr ||
      ScalarRoot(row_iv.bound_) != ScalarRoot(column_iv.bound_) ||
      column_loop->GetBodyRegion()->blocks_.size() != 1) {
    return false;
  }

  auto *column_body = column_loop->GetBodyRegion()->blocks_[0];
  auto direct = DirectLoops(column_body);
  if (direct.size() != 1 || direct[0]->GetResultsSize() != 2) {
    return false;
  }
  auto *row_kernel = direct[0];
  auto row_kernel_iv = loopiv::Recognize(row_kernel);
  int64_t kernel_size = loopiv::ConstantTripCount(
      row_kernel_iv, std::numeric_limits<int32_t>::max());
  if (!row_kernel_iv.valid_ || row_kernel_iv.kind_ != loopiv::CmpKind::kLT ||
      row_kernel_iv.step_kind_ != loopiv::StepKind::kAdd ||
      row_kernel_iv.step_ != 1 || !IsZero(row_kernel_iv.init_) ||
      kernel_size < 3 || kernel_size > 15 || kernel_size % 2 == 0 ||
      row_kernel->GetBodyRegion()->blocks_.size() != 1) {
    return false;
  }
  auto *row_kernel_body = row_kernel->GetBodyRegion()->blocks_[0];
  auto inner = DirectLoops(row_kernel_body);
  if (inner.size() != 1 || inner[0]->GetResultsSize() != 2) {
    return false;
  }
  auto *column_kernel = inner[0];
  auto column_kernel_iv = loopiv::Recognize(column_kernel);
  if (!column_kernel_iv.valid_ ||
      loopiv::ConstantTripCount(column_kernel_iv,
                                std::numeric_limits<int32_t>::max()) != kernel_size ||
      column_kernel_iv.kind_ != loopiv::CmpKind::kLT ||
      column_kernel_iv.step_kind_ != loopiv::StepKind::kAdd ||
      column_kernel_iv.step_ != 1 || !IsZero(column_kernel_iv.init_) ||
      !NoWritesOrCalls(row_kernel->GetBodyRegion())) {
    return false;
  }

  size_t row_accumulator = 1 - row_kernel_iv.index_;
  size_t column_accumulator = 1 - column_kernel_iv.index_;
  auto *row_yield = mlir::DynamicOrNull<mlir::scf::YieldOp>(
      row_kernel_body->operations_.back());
  auto *column_kernel_body = column_kernel->GetBodyRegion()->blocks_[0];
  auto *column_yield = mlir::DynamicOrNull<mlir::scf::YieldOp>(
      column_kernel_body->operations_.back());
  if (row_yield == nullptr || column_yield == nullptr ||
      !Same(row_yield->GetOperands()[row_accumulator],
            column_kernel->GetResultByIndex(column_accumulator))) {
    return false;
  }
  auto *guard_value = TraceCopies(column_yield->GetOperands()[column_accumulator]);
  auto *guarded = guard_value == nullptr
                      ? nullptr
                      : mlir::DynamicOrNull<mlir::scf::IfOp>(guard_value->op_);
  if (guarded == nullptr || guarded->results_.size() != 1) {
    return false;
  }
  auto *guard_else = RegionYield(guarded->GetElseRegion());
  if (guard_else == nullptr || guard_else->GetOperands().size() != 1 ||
      !Same(guard_else->GetOperands()[0],
            column_kernel_body->arguments_[column_accumulator])) {
    return false;
  }

  std::vector<mlir::Value *> leaves;
  std::vector<mlir::scf::IfOp *> logical_ands;
  if (!FlattenAnd(guarded->GetCondition(), &leaves, &logical_ands) ||
      leaves.size() != 4 || logical_ands.size() != 3) {
    return false;
  }
  struct Bounds {
    mlir::Value *coordinate{nullptr};
    mlir::var::VarGetValOp *bound{nullptr};
    bool lower{false};
    bool upper{false};
  };
  std::vector<Bounds> bounds;
  for (auto *leaf : leaves) {
    if (auto *lower = mlir::DynamicOrNull<mlir::arithm::GreaterEqualOp>(
            TraceCopies(leaf)->op_)) {
      if (!IsZero(lower->GetRHS())) {
        return false;
      }
      bounds.push_back({TraceCopies(lower->GetLHS()), nullptr, true, false});
      continue;
    }
    auto *upper =
        mlir::DynamicOrNull<mlir::arithm::LessOp>(TraceCopies(leaf)->op_);
    auto *load = upper == nullptr ? nullptr : ScalarLoad(upper->GetRHS());
    if (load == nullptr ||
        analysis::MLIRMemoryEffectAnalysis::GetPointerRoot(load->GetPtr()).value !=
            ScalarRoot(row_iv.bound_)) {
      return false;
    }
    bool merged = false;
    for (auto &entry : bounds) {
      if (Same(entry.coordinate, upper->GetLHS())) {
        entry.upper = true;
        entry.bound = load;
        merged = true;
        break;
      }
    }
    if (!merged) {
      bounds.push_back(
          {TraceCopies(upper->GetLHS()), load, false, true});
    }
  }
  for (size_t left = 0; left < bounds.size(); ++left) {
    for (size_t right = left + 1; right < bounds.size();) {
      if (!Same(bounds[left].coordinate, bounds[right].coordinate)) {
        ++right;
        continue;
      }
      bounds[left].lower = bounds[left].lower || bounds[right].lower;
      bounds[left].upper = bounds[left].upper || bounds[right].upper;
      if (bounds[left].bound == nullptr) {
        bounds[left].bound = bounds[right].bound;
      }
      bounds.erase(bounds.begin() + static_cast<std::ptrdiff_t>(right));
    }
  }
  if (bounds.size() != 2 || !bounds[0].lower || !bounds[0].upper ||
      !bounds[1].lower || !bounds[1].upper) {
    return false;
  }

  auto *row = row_loop->GetBodyRegion()->blocks_[0]->arguments_[row_iv.index_];
  auto *column = column_body->arguments_[column_iv.index_];
  auto *kr =
      row_kernel_body->arguments_[row_kernel_iv.index_];
  auto *kc =
      column_kernel_body->arguments_[column_kernel_iv.index_];
  int32_t row_pad = 0;
  int32_t column_pad = 0;
  mlir::var::VarGetValOp *bound_load = nullptr;
  for (auto &entry : bounds) {
    int32_t pad = 0;
    if (Coordinate(entry.coordinate, row, kr, &pad)) {
      row_pad = pad;
      bound_load = entry.bound;
    } else if (Coordinate(entry.coordinate, column, kc, &pad)) {
      column_pad = pad;
      if (bound_load == nullptr) {
        bound_load = entry.bound;
      }
    } else {
      return false;
    }
  }
  if (row_pad != column_pad || row_pad < 0 ||
      row_pad >= kernel_size ||
      kernel_size - 1 - row_pad < 0 || bound_load == nullptr) {
    return false;
  }

  mlir::var::VarSetOp *output_store = nullptr;
  for (auto *operation : column_body->operations_) {
    auto *store = mlir::DynamicOrNull<mlir::var::VarSetOp>(operation);
    if (store == nullptr) {
      continue;
    }
    if (output_store != nullptr ||
        !Same(store->GetNewValue(),
              row_kernel->GetResultByIndex(row_accumulator))) {
      return false;
    }
    output_store = store;
  }
  if (output_store == nullptr ||
      !row_kernel->GetResultByIndex(1 - row_accumulator)->GetUses().empty()) {
    return false;
  }

  candidate->function = function;
  candidate->column_body = column_body;
  candidate->row_loop = row_loop;
  candidate->column_loop = column_loop;
  candidate->row_kernel_loop = row_kernel;
  candidate->column_kernel_loop = column_kernel;
  candidate->guarded_update = guarded;
  candidate->logical_ands = std::move(logical_ands);
  candidate->bound_load = bound_load;
  candidate->output_store = output_store;
  candidate->row = row;
  candidate->column = column;
  candidate->row_kernel_accumulator = row_accumulator;
  candidate->pad = row_pad;
  candidate->maximum_offset =
      static_cast<int32_t>(kernel_size - 1 - row_pad);
  return true;
}

auto MLIRStencilInteriorPass::Specialize(const Candidate &candidate) -> void {
  cloner_.value_map_.clear();
  auto *fast_loop = mlir::DynamicOrNull<mlir::scf::WhileOp>(
      cloner_.Clone(candidate.row_kernel_loop));
  auto *fast_guard = mlir::DynamicOrNull<mlir::scf::IfOp>(
      cloner_.RemapValue(candidate.guarded_update->GetResultByIndex(0))->op_);
  auto *fast_column_kernel = mlir::DynamicOrNull<mlir::scf::WhileOp>(
      cloner_.RemapValue(candidate.column_kernel_loop->GetResultByIndex(0))->op_);

  auto *true_integer = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::ConstOp>(
      GET_MLIR_CONTEXT(), static_cast<int>(true));
  true_integer->GetRes()->SetType(GET_TYPE_CONTEXT()->GetIntegerType());
  auto *true_constant = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::CastOp>(
      GET_MLIR_CONTEXT(), true_integer->GetRes(),
      GET_TYPE_CONTEXT()->GetIntegerType(),
      candidate.guarded_update->GetCondition()->GetType());
  true_constant->GetRes()->SetType(
      candidate.guarded_update->GetCondition()->GetType());
  fast_guard->operands_[0] = true_constant->GetRes();

  auto *fast_inner_body = fast_column_kernel->GetBodyRegion()->blocks_[0];
  for (auto *logical : candidate.logical_ands) {
    auto *cloned = mlir::DynamicOrNull<mlir::scf::IfOp>(
        cloner_.RemapValue(logical->GetResultByIndex(0))->op_);
    fast_inner_body->operations_.erase(
        std::find(fast_inner_body->operations_.begin(),
                  fast_inner_body->operations_.end(), cloned));
  }

  auto *original_pointer = mlir::DynamicOrNull<mlir::var::VarGetPtrOp>(
      TraceCopies(candidate.bound_load->GetPtr())->op_);
  auto *bound_pointer = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarGetPtrOp>(
      GET_MLIR_CONTEXT(), original_pointer->GetPtr(),
      original_pointer->GetIndices());
  bound_pointer->GetRes()->SetType(original_pointer->GetRes()->GetType());
  auto *bound_load = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarGetValOp>(
      GET_MLIR_CONTEXT(), bound_pointer->GetRes());
  bound_load->GetRes()->SetType(candidate.bound_load->GetRes()->GetType());

  auto *pad = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::ConstOp>(
      GET_MLIR_CONTEXT(), candidate.pad);
  pad->GetRes()->SetType(candidate.row->GetType());
  auto *maximum_offset =
      GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::ConstOp>(
          GET_MLIR_CONTEXT(), candidate.maximum_offset);
  maximum_offset->GetRes()->SetType(candidate.row->GetType());
  auto *upper = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::SubOp>(
      GET_MLIR_CONTEXT(), bound_load->GetRes(), maximum_offset->GetRes());
  upper->GetRes()->SetType(candidate.row->GetType());

  auto comparison = [&](auto operation, mlir::Value *lhs, mlir::Value *rhs) {
    auto *result = operation(lhs, rhs);
    result->GetRes()->SetType(
        candidate.guarded_update->GetCondition()->GetType());
    return result;
  };
  auto *row_lower = comparison(
      [&](mlir::Value *lhs, mlir::Value *rhs) {
        return GET_MLIR_CONTEXT()
            ->CreateOperation<mlir::arithm::GreaterEqualOp>(
                GET_MLIR_CONTEXT(), lhs, rhs);
      },
      candidate.row, pad->GetRes());
  auto *row_upper = comparison(
      [&](mlir::Value *lhs, mlir::Value *rhs) {
        return GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::LessOp>(
            GET_MLIR_CONTEXT(), lhs, rhs);
      },
      candidate.row, upper->GetRes());
  auto *column_lower = comparison(
      [&](mlir::Value *lhs, mlir::Value *rhs) {
        return GET_MLIR_CONTEXT()
            ->CreateOperation<mlir::arithm::GreaterEqualOp>(
                GET_MLIR_CONTEXT(), lhs, rhs);
      },
      candidate.column, pad->GetRes());
  auto *column_upper = comparison(
      [&](mlir::Value *lhs, mlir::Value *rhs) {
        return GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::LessOp>(
            GET_MLIR_CONTEXT(), lhs, rhs);
      },
      candidate.column, upper->GetRes());

  auto make_and = [&](mlir::Value *lhs, mlir::Value *rhs) {
    auto *operation = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::IfOp>(
        GET_MLIR_CONTEXT(), 1, lhs,
        MakeYieldRegion(GET_MLIR_CONTEXT(), nullptr, rhs),
        MakeYieldRegion(GET_MLIR_CONTEXT(), nullptr, lhs));
    operation->GetResultByIndex(0)->SetType(lhs->GetType());
    return operation;
  };
  auto *row_inside = make_and(row_lower->GetRes(), row_upper->GetRes());
  auto *column_inside =
      make_and(column_lower->GetRes(), column_upper->GetRes());
  auto *inside =
      make_and(row_inside->GetResultByIndex(0),
               column_inside->GetResultByIndex(0));

  auto *fast_result =
      fast_loop->GetResultByIndex(candidate.row_kernel_accumulator);
  auto *slow_result = candidate.row_kernel_loop->GetResultByIndex(
      candidate.row_kernel_accumulator);
  auto *versioned = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::IfOp>(
      GET_MLIR_CONTEXT(), 1, inside->GetResultByIndex(0),
      MakeYieldRegion(GET_MLIR_CONTEXT(), fast_loop, fast_result),
      MakeYieldRegion(GET_MLIR_CONTEXT(), candidate.row_kernel_loop,
                      slow_result));
  versioned->GetResultByIndex(0)->SetType(slow_result->GetType());

  slow_result->ReplaceAllUsesWith(versioned->GetResultByIndex(0));
  // The replacement above must not rewrite the slow branch's newly-created
  // yield back to the versioned result.
  RegionYield(versioned->GetElseRegion())->operands_[0] = slow_result;

  std::vector<mlir::Operation *> prefix{
      bound_pointer, bound_load, true_integer, true_constant, pad,
      maximum_offset, upper, row_lower, row_upper, column_lower,
      column_upper, row_inside, column_inside, inside, versioned};
  auto position =
      std::find(candidate.column_body->operations_.begin(),
                candidate.column_body->operations_.end(),
                candidate.row_kernel_loop);
  auto index = static_cast<size_t>(
      std::distance(candidate.column_body->operations_.begin(), position));
  candidate.column_body->operations_.erase(position);
  candidate.column_body->operations_.insert(
      candidate.column_body->operations_.begin() +
          static_cast<std::ptrdiff_t>(index),
      prefix.begin(), prefix.end());
}

}  // namespace middleend::pass::transform
