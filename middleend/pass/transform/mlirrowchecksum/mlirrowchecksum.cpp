#include "middleend/pass/transform/mlirrowchecksum/mlirrowchecksum.h"
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

struct Shape {
  loopiv::Induction iv;
  mlir::Block *condition_block{nullptr};
  mlir::Block *body{nullptr};
  mlir::scf::ConditionOp *condition{nullptr};
  mlir::scf::YieldOp *yield{nullptr};
};

auto Trace(mlir::Value *value) -> mlir::Value * {
  while (value != nullptr) {
    auto *copy = mlir::DynamicOrNull<mlir::arithm::CopyOp>(value->op_);
    if (copy == nullptr) break;
    value = copy->GetValue();
  }
  return value;
}

auto Same(mlir::Value *lhs, mlir::Value *rhs) -> bool {
  return Trace(lhs) == Trace(rhs);
}

auto IsZero(mlir::Value *value) -> bool {
  value = Trace(value);
  auto *constant =
      value == nullptr ? nullptr : mlir::DynamicOrNull<mlir::arithm::ConstOp>(value->op_);
  return constant != nullptr && std::holds_alternative<int>(constant->GetValue()) &&
         std::get<int>(constant->GetValue()) == 0;
}

auto LoopShape(mlir::scf::WhileOp *loop) -> Shape {
  Shape shape;
  if (loop == nullptr || loop->GetCondRegion()->blocks_.size() != 1 ||
      loop->GetBodyRegion()->blocks_.size() != 1) {
    return shape;
  }
  shape.iv = loopiv::Recognize(loop);
  shape.condition_block = loop->GetCondRegion()->blocks_[0];
  shape.body = loop->GetBodyRegion()->blocks_[0];
  shape.condition = mlir::DynamicOrNull<mlir::scf::ConditionOp>(
      shape.condition_block->operations_.back());
  shape.yield =
      mlir::DynamicOrNull<mlir::scf::YieldOp>(shape.body->operations_.back());
  if (!shape.iv.valid_ || shape.iv.kind_ != loopiv::CmpKind::kLT ||
      shape.iv.step_kind_ != loopiv::StepKind::kAdd || shape.iv.step_ != 1 ||
      !IsZero(shape.iv.init_) || shape.condition == nullptr ||
      shape.yield == nullptr) {
    shape.iv.valid_ = false;
  }
  return shape;
}

auto Root(mlir::Value *pointer) -> mlir::Value * {
  return analysis::MLIRMemoryEffectAnalysis::GetPointerRoot(pointer).value;
}

auto ScalarLoad(mlir::Value *value) -> mlir::var::VarGetValOp * {
  value = Trace(value);
  auto *load =
      value == nullptr ? nullptr : mlir::DynamicOrNull<mlir::var::VarGetValOp>(value->op_);
  auto *traced_pointer = load == nullptr ? nullptr : Trace(load->GetPtr());
  auto *pointer =
      traced_pointer == nullptr
          ? nullptr
          : mlir::DynamicOrNull<mlir::var::VarGetPtrOp>(traced_pointer->op_);
  return pointer != nullptr && pointer->GetIndices().empty() ? load : nullptr;
}

auto SameBound(mlir::Value *value, mlir::Value *root) -> bool {
  auto *load = ScalarLoad(value);
  return load != nullptr && Root(load->GetPtr()) == root;
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

auto NextLoop(mlir::Block *block, size_t start, size_t *index) -> mlir::scf::WhileOp * {
  for (size_t current = start; current < block->operations_.size(); ++current) {
    if (auto *loop =
            mlir::DynamicOrNull<mlir::scf::WhileOp>(block->operations_[current])) {
      *index = current;
      return loop;
    }
    if (mlir::DynamicOrNull<mlir::arithm::CopyOp>(block->operations_[current]) == nullptr &&
        mlir::DynamicOrNull<mlir::arithm::ConstOp>(block->operations_[current]) == nullptr &&
        mlir::DynamicOrNull<mlir::var::VarGetPtrOp>(block->operations_[current]) == nullptr) {
      return nullptr;
    }
  }
  return nullptr;
}

auto FlatIndex(mlir::Value *index, mlir::Value *row, mlir::Value *column,
               mlir::Value *bound_root) -> bool {
  index = Trace(index);
  auto *add =
      index == nullptr ? nullptr : mlir::DynamicOrNull<mlir::arithm::AddOp>(index->op_);
  if (add == nullptr) return false;
  mlir::Value *product_value = Same(add->GetLHS(), column) ? add->GetRHS()
                                                           : add->GetLHS();
  if (!Same(add->GetLHS(), column) && !Same(add->GetRHS(), column)) return false;
  product_value = Trace(product_value);
  auto *product =
      product_value == nullptr
          ? nullptr
          : mlir::DynamicOrNull<mlir::arithm::MulOp>(product_value->op_);
  if (product == nullptr) return false;
  mlir::Value *bound = Same(product->GetLHS(), row) ? product->GetRHS()
                                                    : product->GetLHS();
  return (Same(product->GetLHS(), row) || Same(product->GetRHS(), row)) &&
         SameBound(bound, bound_root);
}

auto AccessIndex(mlir::Value *pointer, mlir::Value *root,
                 mlir::Value **index) -> bool {
  pointer = Trace(pointer);
  auto *get =
      pointer == nullptr ? nullptr : mlir::DynamicOrNull<mlir::var::VarGetPtrOp>(pointer->op_);
  if (get == nullptr || Root(get->GetRes()) != root ||
      get->GetIndices().size() != 1) {
    return false;
  }
  *index = get->GetIndices()[0];
  return true;
}

auto SquareBound(mlir::Value *value, mlir::Value *bound_root) -> bool {
  value = Trace(value);
  auto *multiply =
      value == nullptr ? nullptr : mlir::DynamicOrNull<mlir::arithm::MulOp>(value->op_);
  return multiply != nullptr && SameBound(multiply->GetLHS(), bound_root) &&
         SameBound(multiply->GetRHS(), bound_root);
}

auto SafeTrailingCall(mlir::func::FuncCallOp *call) -> bool {
  if (call == nullptr) return true;
  const auto &name = call->GetName();
  return name == "_sysy_stoptime" || name == "putint" || name == "putch" ||
         name == "putfloat";
}

auto OperationUsesRoot(mlir::Operation *operation, mlir::Value *root) -> bool;

auto RegionUsesRoot(mlir::Region *region, mlir::Value *root) -> bool {
  for (auto *block : region->blocks_) {
    for (auto *operation : block->operations_) {
      if (OperationUsesRoot(operation, root)) return true;
    }
  }
  return false;
}

auto OperationUsesRoot(mlir::Operation *operation, mlir::Value *root) -> bool {
  if (auto *get = mlir::DynamicOrNull<mlir::var::VarGetPtrOp>(operation)) {
    if (Root(get->GetRes()) == root) return true;
  }
  if (!SafeTrailingCall(
          mlir::DynamicOrNull<mlir::func::FuncCallOp>(operation))) {
    return true;
  }
  for (auto *nested : operation->regions_) {
    if (RegionUsesRoot(nested, root)) return true;
  }
  return false;
}

auto YieldRegion(context::MLIRContext *context, mlir::Operation *first,
                 mlir::Operation *second, mlir::Value *value) -> mlir::Region * {
  auto *block = new mlir::Block();
  if (first != nullptr) block->AddOperation(first);
  if (second != nullptr) block->AddOperation(second);
  block->AddOperation(context->CreateOperation<mlir::scf::YieldOp>(
      context, std::vector<mlir::Value *>{value}));
  auto *region = new mlir::Region();
  region->AddBlock(block);
  return region;
}

}  // namespace

auto MLIRRowChecksumPass::Run() -> void {
  PreRun();
  if (std::getenv("ROW_CHECKSUM_OFF") != nullptr) return;
  candidates_ = 0;
  fused_ = 0;
  auto *module = GET_MLIR_CONTEXT()->GetModuleOp();
  if (module == nullptr) throw std::logic_error("MLIR module is not set");
  std::unordered_set<std::string> called;
  for (auto *operation : module->GetBlock()->operations_) {
    if (auto *function = mlir::DynamicOrNull<mlir::func::FuncDefOp>(operation)) {
      CollectCalledFunctions(function->GetEntryBlock(), &called);
    }
  }
  for (auto *operation : module->GetBlock()->operations_) {
    auto *function = mlir::DynamicOrNull<mlir::func::FuncDefOp>(operation);
    if (function == nullptr) continue;
    mlir::utils::RebuildFunctionUses(function);
    if (FindInBlock(function, function->GetEntryBlock(), called)) {
      mlir::utils::RebuildFunctionUses(function);
    }
  }
  if (std::getenv("LOOP_OPT_STATS") != nullptr) {
    std::cerr << "RowChecksum: candidates=" << candidates_ << " fused=" << fused_
              << '\n';
  }
}

auto MLIRRowChecksumPass::CollectCalledFunctions(
    mlir::Block *block, std::unordered_set<std::string> *called) -> void {
  for (auto *operation : block->operations_) {
    if (auto *call = mlir::DynamicOrNull<mlir::func::FuncCallOp>(operation)) {
      called->insert(call->GetName());
    }
    for (auto *region : operation->regions_) {
      for (auto *nested : region->blocks_) CollectCalledFunctions(nested, called);
    }
  }
}

auto MLIRRowChecksumPass::FindInBlock(
    mlir::func::FuncDefOp *function, mlir::Block *block,
    const std::unordered_set<std::string> &called) -> bool {
  if (called.count(function->GetName()) == 0) {
    for (size_t index = 0; index < block->operations_.size(); ++index) {
      Candidate candidate;
      if (FindCandidate(function, block, index, &candidate)) {
        ++candidates_;
        Fuse(candidate);
        ++fused_;
        return true;
      }
    }
  }
  for (auto *operation : block->operations_) {
    for (auto *region : operation->regions_) {
      for (auto *nested : region->blocks_) {
        if (FindInBlock(function, nested, called)) return true;
      }
    }
  }
  return false;
}

auto MLIRRowChecksumPass::FindCandidate(
    mlir::func::FuncDefOp *function, mlir::Block *parent, size_t row_index,
    Candidate *candidate) -> bool {
  auto *row_loop =
      mlir::DynamicOrNull<mlir::scf::WhileOp>(parent->operations_[row_index]);
  auto row = LoopShape(row_loop);
  auto *bound_load = row.iv.valid_ ? ScalarLoad(row.iv.bound_) : nullptr;
  if (!row.iv.valid_ || bound_load == nullptr) return false;
  auto *bound_root = Root(bound_load->GetPtr());
  auto direct = DirectLoops(row.body);
  if (direct.size() != 2) return false;
  auto *sum_loop = direct[0];
  auto *update_loop = direct[1];
  auto sum = LoopShape(sum_loop);
  auto update = LoopShape(update_loop);
  if (!sum.iv.valid_ || !update.iv.valid_ ||
      !SameBound(sum.iv.bound_, bound_root) ||
      !SameBound(update.iv.bound_, bound_root) ||
      sum_loop->GetResultsSize() != 2) {
    return false;
  }
  size_t sum_accumulator = 1 - sum.iv.index_;
  auto sum_original = sum.condition->GetOriginalValues();
  if (sum_original.size() != 2 || !IsZero(sum_original[sum_accumulator])) {
    return false;
  }
  auto *row_value = row.body->arguments_[row.iv.index_];
  auto *sum_column = sum.body->arguments_[sum.iv.index_];
  auto *sum_argument = sum.body->arguments_[sum_accumulator];
  mlir::var::VarGetValOp *sum_load = nullptr;
  mlir::arithm::AddOp *sum_add = nullptr;
  for (auto *operation : sum.body->operations_) {
    if (!operation->regions_.empty()) return false;
    if (auto *load = mlir::DynamicOrNull<mlir::var::VarGetValOp>(operation)) {
      auto *pointer = Trace(load->GetPtr());
      auto *get =
          pointer == nullptr
              ? nullptr
              : mlir::DynamicOrNull<mlir::var::VarGetPtrOp>(pointer->op_);
      // Scalar dimension loads may remain in the flattened index expression.
      // Only the single indexed array load belongs to the row reduction.
      if (get == nullptr || get->GetIndices().size() != 1) continue;
      if (sum_load != nullptr) return false;
      sum_load = load;
    } else if (auto *add = mlir::DynamicOrNull<mlir::arithm::AddOp>(operation)) {
      if ((Same(add->GetLHS(), sum_argument) &&
           sum_load != nullptr && Same(add->GetRHS(), sum_load->GetRes())) ||
          (Same(add->GetRHS(), sum_argument) &&
           sum_load != nullptr && Same(add->GetLHS(), sum_load->GetRes()))) {
        sum_add = add;
      }
    } else if (mlir::DynamicOrNull<mlir::var::VarSetOp>(operation) != nullptr ||
               mlir::DynamicOrNull<mlir::func::FuncCallOp>(operation) != nullptr) {
      return false;
    }
  }
  mlir::Value *sum_index_value = nullptr;
  auto *root = sum_load == nullptr ? nullptr : Root(sum_load->GetPtr());
  if (root == nullptr || sum_add == nullptr ||
      !Same(sum.yield->GetOperands()[sum_accumulator], sum_add->GetRes()) ||
      !AccessIndex(sum_load->GetPtr(), root, &sum_index_value) ||
      !FlatIndex(sum_index_value, row_value, sum_column, bound_root)) {
    return false;
  }

  auto *update_column = update.body->arguments_[update.iv.index_];
  mlir::var::VarGetValOp *update_load = nullptr;
  mlir::var::VarSetOp *update_store = nullptr;
  mlir::arithm::SubOp *subtract = nullptr;
  for (auto *operation : update.body->operations_) {
    if (!operation->regions_.empty()) return false;
    if (auto *load = mlir::DynamicOrNull<mlir::var::VarGetValOp>(operation)) {
      if (Root(load->GetPtr()) == root) {
        if (update_load != nullptr) return false;
        update_load = load;
      }
    } else if (auto *store = mlir::DynamicOrNull<mlir::var::VarSetOp>(operation)) {
      if (update_store != nullptr) return false;
      update_store = store;
    } else if (auto *sub = mlir::DynamicOrNull<mlir::arithm::SubOp>(operation)) {
      subtract = sub;
    } else if (mlir::DynamicOrNull<mlir::func::FuncCallOp>(operation) != nullptr) {
      return false;
    }
  }
  mlir::Value *load_index = nullptr;
  mlir::Value *store_index = nullptr;
  if (update_load == nullptr || update_store == nullptr || subtract == nullptr ||
      !Same(subtract->GetLHS(), update_load->GetRes()) ||
      !Same(subtract->GetRHS(),
            sum_loop->GetResultByIndex(sum_accumulator)) ||
      !Same(update_store->GetNewValue(), subtract->GetRes()) ||
      !AccessIndex(update_load->GetPtr(), root, &load_index) ||
      !AccessIndex(update_store->GetPtr(), root, &store_index) ||
      !FlatIndex(load_index, row_value, update_column, bound_root) ||
      !FlatIndex(store_index, row_value, update_column, bound_root)) {
    return false;
  }
  for (auto *result : row_loop->results_) {
    if (!result->GetUses().empty()) return false;
  }
  for (auto *result : update_loop->results_) {
    if (!result->GetUses().empty()) return false;
  }

  size_t checksum_index = 0;
  auto *checksum_loop = NextLoop(parent, row_index + 1, &checksum_index);
  auto checksum = LoopShape(checksum_loop);
  if (!checksum.iv.valid_ || checksum_loop->GetResultsSize() != 2 ||
      !SquareBound(checksum.iv.bound_, bound_root)) {
    return false;
  }
  size_t checksum_accumulator = 1 - checksum.iv.index_;
  auto checksum_original = checksum.condition->GetOriginalValues();
  if (checksum_original.size() != 2 ||
      !IsZero(checksum_original[checksum_accumulator]) ||
      checksum_loop->GetResultByIndex(checksum_accumulator)->GetUses().empty() ||
      !checksum_loop->GetResultByIndex(checksum.iv.index_)->GetUses().empty()) {
    return false;
  }
  mlir::var::VarGetValOp *checksum_load = nullptr;
  mlir::arithm::AddOp *checksum_add = nullptr;
  auto *checksum_argument =
      checksum.body->arguments_[checksum_accumulator];
  for (auto *operation : checksum.body->operations_) {
    if (!operation->regions_.empty()) return false;
    if (auto *load = mlir::DynamicOrNull<mlir::var::VarGetValOp>(operation)) {
      auto *pointer = Trace(load->GetPtr());
      auto *get =
          pointer == nullptr
              ? nullptr
              : mlir::DynamicOrNull<mlir::var::VarGetPtrOp>(pointer->op_);
      if (get == nullptr || get->GetIndices().size() != 1) continue;
      if (checksum_load != nullptr) return false;
      checksum_load = load;
    } else if (auto *add = mlir::DynamicOrNull<mlir::arithm::AddOp>(operation)) {
      if ((Same(add->GetLHS(), checksum_argument) &&
           checksum_load != nullptr &&
           Same(add->GetRHS(), checksum_load->GetRes())) ||
          (Same(add->GetRHS(), checksum_argument) &&
           checksum_load != nullptr &&
           Same(add->GetLHS(), checksum_load->GetRes()))) {
        checksum_add = add;
      }
    } else if (mlir::DynamicOrNull<mlir::var::VarSetOp>(operation) != nullptr ||
               mlir::DynamicOrNull<mlir::func::FuncCallOp>(operation) != nullptr) {
      return false;
    }
  }
  mlir::Value *checksum_access = nullptr;
  if (checksum_load == nullptr || Root(checksum_load->GetPtr()) != root ||
      checksum_add == nullptr ||
      !Same(checksum.yield->GetOperands()[checksum_accumulator],
            checksum_add->GetRes()) ||
      !AccessIndex(checksum_load->GetPtr(), root, &checksum_access) ||
      !Same(checksum_access,
            checksum.body->arguments_[checksum.iv.index_])) {
    return false;
  }

  bool observable = false;
  for (size_t index = checksum_index + 1; index < parent->operations_.size();
       ++index) {
    if (OperationUsesRoot(parent->operations_[index], root)) {
      observable = true;
      break;
    }
  }
  if (observable) return false;

  candidate->function = function;
  candidate->parent = parent;
  candidate->row_loop = row_loop;
  candidate->sum_loop = sum_loop;
  candidate->update_loop = update_loop;
  candidate->checksum_loop = checksum_loop;
  candidate->bound_load = bound_load;
  candidate->root = root;
  candidate->row_index = row_index;
  candidate->checksum_index = checksum_index;
  candidate->sum_accumulator = sum_accumulator;
  candidate->checksum_accumulator = checksum_accumulator;
  return true;
}

auto MLIRRowChecksumPass::Fuse(const Candidate &candidate) -> void {
  cloner_.value_map_.clear();
  auto *fast_row = mlir::DynamicOrNull<mlir::scf::WhileOp>(
      cloner_.Clone(candidate.row_loop));
  auto *fast_sum = mlir::DynamicOrNull<mlir::scf::WhileOp>(
      cloner_.RemapValue(candidate.sum_loop->GetResultByIndex(0))->op_);
  auto *fast_update = mlir::DynamicOrNull<mlir::scf::WhileOp>(
      cloner_.RemapValue(candidate.update_loop->GetResultByIndex(0))->op_);
  auto fast = LoopShape(fast_row);
  auto *fast_row_sum =
      fast_sum->GetResultByIndex(candidate.sum_accumulator);

  auto *original_pointer = mlir::DynamicOrNull<mlir::var::VarGetPtrOp>(
      Trace(candidate.bound_load->GetPtr())->op_);
  auto *bound_pointer =
      GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarGetPtrOp>(
          GET_MLIR_CONTEXT(), original_pointer->GetPtr(),
          original_pointer->GetIndices());
  bound_pointer->GetRes()->SetType(original_pointer->GetRes()->GetType());
  auto *bound_load =
      GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarGetValOp>(
          GET_MLIR_CONTEXT(), bound_pointer->GetRes());
  bound_load->GetRes()->SetType(candidate.bound_load->GetRes()->GetType());
  auto *zero = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::ConstOp>(
      GET_MLIR_CONTEXT(), 0);
  zero->GetRes()->SetType(bound_load->GetRes()->GetType());
  auto *one = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::ConstOp>(
      GET_MLIR_CONTEXT(), 1);
  one->GetRes()->SetType(bound_load->GetRes()->GetType());
  auto *limit = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::ConstOp>(
      GET_MLIR_CONTEXT(), 46340);
  limit->GetRes()->SetType(bound_load->GetRes()->GetType());
  auto *positive =
      GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::GreaterOp>(
          GET_MLIR_CONTEXT(), bound_load->GetRes(), zero->GetRes());
  auto *checksum_condition = mlir::DynamicOrNull<mlir::scf::ConditionOp>(
      candidate.checksum_loop->GetCondRegion()
          ->blocks_[0]
          ->operations_.back());
  positive->GetRes()->SetType(checksum_condition->GetCondition()->GetType());
  auto *bounded =
      GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::LessEqualOp>(
          GET_MLIR_CONTEXT(), bound_load->GetRes(), limit->GetRes());
  bounded->GetRes()->SetType(positive->GetRes()->GetType());
  auto *safe = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::IfOp>(
      GET_MLIR_CONTEXT(), 1, positive->GetRes(),
      YieldRegion(GET_MLIR_CONTEXT(), nullptr, nullptr, bounded->GetRes()),
      YieldRegion(GET_MLIR_CONTEXT(), nullptr, nullptr, positive->GetRes()));
  safe->GetResultByIndex(0)->SetType(positive->GetRes()->GetType());
  auto *factor = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::SubOp>(
      GET_MLIR_CONTEXT(), one->GetRes(), bound_load->GetRes());
  factor->GetRes()->SetType(bound_load->GetRes()->GetType());

  auto *accumulator = GET_MLIR_CONTEXT()->CreateValue();
  accumulator->SetType(zero->GetRes()->GetType());
  fast.condition_block->AddArgument(accumulator);
  fast.body->AddArgument(accumulator);
  auto carried = fast.condition->GetOperands();
  auto originals = fast.condition->GetOriginalValues();
  carried.emplace_back(accumulator);
  originals.emplace_back(zero->GetRes());
  fast.condition->SetLoopCarriedValues(carried, originals);
  auto *scaled = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::MulOp>(
      GET_MLIR_CONTEXT(), fast_row_sum, factor->GetRes());
  scaled->GetRes()->SetType(accumulator->GetType());
  auto *next = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::AddOp>(
      GET_MLIR_CONTEXT(), accumulator, scaled->GetRes());
  next->GetRes()->SetType(accumulator->GetType());
  fast.body->operations_.insert(fast.body->operations_.end() - 1, scaled);
  fast.body->operations_.insert(fast.body->operations_.end() - 1, next);
  fast.yield->operands_.emplace_back(next->GetRes());
  auto *fast_result = GET_MLIR_CONTEXT()->CreateValue();
  fast_result->SetType(accumulator->GetType());
  fast_result->op_ = fast_row;
  fast_row->results_.emplace_back(fast_result);
  fast.body->operations_.erase(std::find(fast.body->operations_.begin(),
                                         fast.body->operations_.end(),
                                         fast_update));

  auto *slow_result = candidate.checksum_loop->GetResultByIndex(
      candidate.checksum_accumulator);
  auto *versioned = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::IfOp>(
      GET_MLIR_CONTEXT(), 1, safe->GetResultByIndex(0),
      YieldRegion(GET_MLIR_CONTEXT(), fast_row, nullptr, fast_result),
      YieldRegion(GET_MLIR_CONTEXT(), candidate.row_loop,
                  candidate.checksum_loop, slow_result));
  versioned->GetResultByIndex(0)->SetType(slow_result->GetType());
  slow_result->ReplaceAllUsesWith(versioned->GetResultByIndex(0));
  auto *else_yield = mlir::DynamicOrNull<mlir::scf::YieldOp>(
      versioned->GetElseRegion()->blocks_[0]->operations_.back());
  else_yield->operands_[0] = slow_result;

  std::vector<mlir::Operation *> prefix{
      bound_pointer, bound_load, zero, one, limit, positive,
      bounded,       safe,       factor, versioned};
  candidate.parent->operations_.erase(
      candidate.parent->operations_.begin() +
      static_cast<std::ptrdiff_t>(candidate.checksum_index));
  candidate.parent->operations_.erase(
      candidate.parent->operations_.begin() +
      static_cast<std::ptrdiff_t>(candidate.row_index));
  // Only constants and copies may occur between the loops. Insert after those
  // definitions so the slow checksum branch cannot capture a later value.
  auto insertion_index = candidate.checksum_index - 1;
  candidate.parent->operations_.insert(
      candidate.parent->operations_.begin() +
          static_cast<std::ptrdiff_t>(insertion_index),
      prefix.begin(), prefix.end());
}

}  // namespace middleend::pass::transform
