#include "middleend/pass/transform/mlirloopunroll/inductionvariable.h"
#include <algorithm>
#include <cstdint>
#include <unordered_set>
#include <utility>
#include <variant>
#include "middleend/mlir/dialect/arithm/binaryop.h"
#include "middleend/mlir/dialect/arithm/copy.h"
#include "middleend/mlir/dialect/arithm/unaryop.h"
#include "middleend/mlir/dialect/func/funcdef.h"
#include "middleend/mlir/dialect/func/return.h"
#include "middleend/mlir/dialect/scf/break.h"
#include "middleend/mlir/dialect/scf/condition.h"
#include "middleend/mlir/dialect/scf/continue.h"
#include "middleend/mlir/dialect/scf/if.h"
#include "middleend/mlir/dialect/scf/while.h"
#include "middleend/mlir/dialect/scf/yield.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/block.h"
#include "middleend/mlir/utils/struct/region.h"
#include "middleend/mlir/utils/value/value.h"

namespace middleend::pass::transform::loopiv {
namespace {

// Swaps the comparison so the induction variable is the left operand when it was on the right.
auto SwapSides(CmpKind kind) -> CmpKind {
  switch (kind) {
    case CmpKind::kLT:
      return CmpKind::kGT;
    case CmpKind::kLE:
      return CmpKind::kGE;
    case CmpKind::kGT:
      return CmpKind::kLT;
    case CmpKind::kGE:
      return CmpKind::kLE;
    case CmpKind::kNE:
      return CmpKind::kNE;
    case CmpKind::kEQ:
      return CmpKind::kEQ;
    default:
      return CmpKind::kInvalid;
  }
}

auto Evaluate(CmpKind kind, int64_t value, int64_t bound) -> bool {
  switch (kind) {
    case CmpKind::kLT:
      return value < bound;
    case CmpKind::kLE:
      return value <= bound;
    case CmpKind::kGT:
      return value > bound;
    case CmpKind::kGE:
      return value >= bound;
    case CmpKind::kNE:
      return value != bound;
    case CmpKind::kEQ:
      return value == bound;
    default:
      return false;
  }
}

// Follows a chain of arithm.copy definitions to the underlying value.
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

// If `value` (after copies) is an integer constant, returns {true, constant}.
auto TraceConstInt(mlir::Value *value) -> std::pair<bool, int64_t> {
  value = TraceCopies(value);
  if (value == nullptr) {
    return {false, 0};
  }
  auto *const_op = mlir::DynamicOrNull<mlir::arithm::ConstOp>(value->op_);
  if (const_op == nullptr) {
    return {false, 0};
  }
  auto raw = const_op->GetValue();
  if (!std::holds_alternative<int>(raw)) {
    return {false, 0};
  }
  return {true, static_cast<int64_t>(std::get<int>(raw))};
}

auto IndexOfArgument(mlir::Value *value, mlir::Block *cond_block) -> int {
  value = TraceCopies(value);
  for (size_t i = 0; i < cond_block->arguments_.size(); ++i) {
    if (cond_block->arguments_[i] == value) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

auto ReadComparison(mlir::Value *cond_value, CmpKind *kind, mlir::Value **lhs, mlir::Value **rhs) -> void {
  cond_value = TraceCopies(cond_value);
  if (cond_value == nullptr) {
    *kind = CmpKind::kInvalid;
    return;
  }
  auto *op = cond_value->op_;
#define READ_CMP(OP_TYPE, KIND)                                     \
  if (auto *cmp = mlir::DynamicOrNull<mlir::arithm::OP_TYPE>(op)) { \
    *kind = KIND;                                                   \
    *lhs = cmp->GetLHS();                                           \
    *rhs = cmp->GetRHS();                                           \
    return;                                                         \
  }
  READ_CMP(LessOp, CmpKind::kLT)
  READ_CMP(LessEqualOp, CmpKind::kLE)
  READ_CMP(GreaterOp, CmpKind::kGT)
  READ_CMP(GreaterEqualOp, CmpKind::kGE)
  READ_CMP(NotEqualOp, CmpKind::kNE)
  READ_CMP(EqualOp, CmpKind::kEQ)
#undef READ_CMP
  *kind = CmpKind::kInvalid;
}

// If `next` (after copies) is `arg + const` / `arg - const` / `arg * const`, fills the step.
auto ExtractStep(mlir::Value *next, mlir::Value *arg, StepKind *step_kind, int64_t *step) -> bool {
  next = TraceCopies(next);
  if (next == nullptr) {
    return false;
  }
  if (auto *add = mlir::DynamicOrNull<mlir::arithm::AddOp>(next->op_)) {
    if (TraceCopies(add->GetLHS()) == arg) {
      auto [ok, value] = TraceConstInt(add->GetRHS());
      if (ok) {
        *step_kind = StepKind::kAdd;
        *step = value;
        return true;
      }
    }
    if (TraceCopies(add->GetRHS()) == arg) {
      auto [ok, value] = TraceConstInt(add->GetLHS());
      if (ok) {
        *step_kind = StepKind::kAdd;
        *step = value;
        return true;
      }
    }
    return false;
  }
  if (auto *sub = mlir::DynamicOrNull<mlir::arithm::SubOp>(next->op_)) {
    if (TraceCopies(sub->GetLHS()) == arg) {
      auto [ok, value] = TraceConstInt(sub->GetRHS());
      if (ok) {
        *step_kind = StepKind::kAdd;
        *step = -value;
        return true;
      }
    }
    return false;
  }
  if (auto *mul = mlir::DynamicOrNull<mlir::arithm::MulOp>(next->op_)) {
    if (TraceCopies(mul->GetLHS()) == arg) {
      auto [ok, value] = TraceConstInt(mul->GetRHS());
      if (ok) {
        *step_kind = StepKind::kMul;
        *step = value;
        return true;
      }
    }
    if (TraceCopies(mul->GetRHS()) == arg) {
      auto [ok, value] = TraceConstInt(mul->GetLHS());
      if (ok) {
        *step_kind = StepKind::kMul;
        *step = value;
        return true;
      }
    }
  }
  return false;
}

auto CollectDefinedInRegion(mlir::Region *region, std::unordered_set<mlir::Value *> *defined) -> void {
  for (auto *block : region->blocks_) {
    for (auto *arg : block->arguments_) {
      defined->insert(arg);
    }
    for (auto *operation : block->operations_) {
      for (auto *result : operation->results_) {
        defined->insert(result);
      }
      for (auto *nested : operation->regions_) {
        CollectDefinedInRegion(nested, defined);
      }
    }
  }
}

}  // namespace

auto Recognize(mlir::scf::WhileOp *while_op) -> Induction {
  Induction iv;
  auto *cond_region = while_op->GetCondRegion();
  auto *body_region = while_op->GetBodyRegion();
  if (cond_region->blocks_.size() != 1 || body_region->blocks_.size() != 1) {
    return iv;
  }
  auto *cond_block = cond_region->blocks_[0];
  auto *body_block = body_region->blocks_[0];
  if (cond_block->operations_.empty() || body_block->operations_.empty()) {
    return iv;
  }
  auto *condition_op = mlir::DynamicOrNull<mlir::scf::ConditionOp>(cond_block->operations_.back());
  auto *yield_op = mlir::DynamicOrNull<mlir::scf::YieldOp>(body_block->operations_.back());
  if (condition_op == nullptr || yield_op == nullptr) {
    return iv;
  }
  auto original_values = condition_op->GetOriginalValues();
  auto &yield_operands = yield_op->GetOperands();
  size_t size = cond_block->arguments_.size();
  if (original_values.size() != size || yield_operands.size() != size) {
    return iv;
  }

  CmpKind kind = CmpKind::kInvalid;
  mlir::Value *lhs = nullptr;
  mlir::Value *rhs = nullptr;
  ReadComparison(condition_op->GetCondition(), &kind, &lhs, &rhs);
  if (kind == CmpKind::kInvalid) {
    return iv;
  }
  int index = IndexOfArgument(lhs, cond_block);
  mlir::Value *bound = rhs;
  if (index < 0) {
    index = IndexOfArgument(rhs, cond_block);
    bound = lhs;
    kind = SwapSides(kind);
    if (index < 0) {
      return iv;
    }
  }

  StepKind step_kind = StepKind::kAdd;
  int64_t step = 0;
  if (!ExtractStep(yield_operands[index], cond_block->arguments_[index], &step_kind, &step)) {
    return iv;
  }

  iv.valid_ = true;
  iv.index_ = static_cast<size_t>(index);
  iv.kind_ = kind;
  iv.step_kind_ = step_kind;
  iv.step_ = step;
  iv.init_ = original_values[index];
  iv.bound_ = bound;
  return iv;
}

auto ConstantTripCount(const Induction &iv, int64_t max_trip) -> int64_t {
  if (!iv.valid_) {
    return -1;
  }
  auto [init_ok, init] = TraceConstInt(iv.init_);
  auto [bound_ok, bound] = TraceConstInt(iv.bound_);
  if (!init_ok || !bound_ok) {
    return -1;
  }
  if (iv.step_kind_ == StepKind::kAdd && iv.step_ != 0) {
    __int128 count = -1;
    __int128 wide_init = init;
    __int128 wide_bound = bound;
    __int128 wide_step = iv.step_;
    switch (iv.kind_) {
      case CmpKind::kLT:
        if (init >= bound) return 0;
        if (iv.step_ <= 0) return -1;
        count = (wide_bound - wide_init + wide_step - 1) / wide_step;
        break;
      case CmpKind::kLE:
        if (init > bound) return 0;
        if (iv.step_ <= 0) return -1;
        count = (wide_bound - wide_init) / wide_step + 1;
        break;
      case CmpKind::kGT:
        if (init <= bound) return 0;
        if (iv.step_ >= 0) return -1;
        count = (wide_init - wide_bound + (-wide_step) - 1) / (-wide_step);
        break;
      case CmpKind::kGE:
        if (init < bound) return 0;
        if (iv.step_ >= 0) return -1;
        count = (wide_init - wide_bound) / (-wide_step) + 1;
        break;
      case CmpKind::kNE: {
        __int128 difference = wide_bound - wide_init;
        if (difference == 0) return 0;
        if (difference % wide_step != 0 || difference / wide_step <= 0) return -1;
        count = difference / wide_step;
        break;
      }
      case CmpKind::kEQ:
        if (init != bound) return 0;
        count = 1;
        break;
      default:
        return -1;
    }
    if (count < 0 || count > max_trip) return -1;
    // The latch update executes after the last body iteration. Proving that
    // value stays in the signed SysY int range also proves no-wrap.
    __int128 exit_value = wide_init + count * wide_step;
    if (exit_value < INT32_MIN || exit_value > INT32_MAX) return -1;
    return static_cast<int64_t>(count);
  }

  int64_t value = init;
  int64_t count = 0;
  while (Evaluate(iv.kind_, value, bound)) {
    ++count;
    if (count > max_trip) {
      return -1;
    }
    if (iv.step_kind_ == StepKind::kAdd) {
      auto wide = static_cast<__int128>(value) + iv.step_;
      if (wide > INT32_MAX || wide < INT32_MIN) {
        return -1;
      }
      value = static_cast<int64_t>(wide);
    } else {
      auto wide = static_cast<__int128>(value) * iv.step_;
      if (wide > INT64_MAX || wide < INT64_MIN) {
        return -1;
      }
      value = static_cast<int64_t>(wide);
    }
  }
  return count;
}

auto IsDefinedInLoop(mlir::Value *value, mlir::scf::WhileOp *while_op) -> bool {
  std::unordered_set<mlir::Value *> defined;
  CollectDefinedInRegion(while_op->GetCondRegion(), &defined);
  CollectDefinedInRegion(while_op->GetBodyRegion(), &defined);
  return defined.count(value) > 0;
}

namespace {
auto OperationIsCloneable(mlir::Operation *operation) -> bool {
  if (mlir::DynamicOrNull<mlir::scf::WhileOp>(operation) != nullptr) {
    return false;
  }
  if (mlir::DynamicOrNull<mlir::scf::BreakOp>(operation) != nullptr ||
      mlir::DynamicOrNull<mlir::scf::ContinueOp>(operation) != nullptr ||
      mlir::DynamicOrNull<mlir::scf::ConditionOp>(operation) != nullptr ||
      mlir::DynamicOrNull<mlir::func::ReturnOp>(operation) != nullptr ||
      mlir::DynamicOrNull<mlir::func::FuncDefOp>(operation) != nullptr) {
    return false;
  }
  if (auto *if_op = mlir::DynamicOrNull<mlir::scf::IfOp>(operation)) {
    for (auto *region : {if_op->GetThenRegion(), if_op->GetElseRegion()}) {
      if (region == nullptr || region->blocks_.size() != 1) {
        return false;
      }
      const auto &operations = region->blocks_[0]->operations_;
      if (!std::all_of(operations.begin(), operations.end(), OperationIsCloneable)) {
        return false;
      }
    }
  }
  return true;
}
}  // namespace

auto BodyIsCloneable(mlir::Region *body) -> bool {
  if (body == nullptr || body->blocks_.size() != 1) {
    return false;
  }
  const auto &operations = body->blocks_[0]->operations_;
  return std::all_of(operations.begin(), operations.end(), OperationIsCloneable);
}

auto CountOperations(mlir::Region *region) -> size_t {
  size_t count = 0;
  for (auto *block : region->blocks_) {
    for (auto *operation : block->operations_) {
      ++count;
      for (auto *nested : operation->regions_) {
        count += CountOperations(nested);
      }
    }
  }
  return count;
}

}  // namespace middleend::pass::transform::loopiv
