#include "middleend/pass/transform/mlirconstantarrayreduction/mlirconstantarrayreduction.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <numeric>
#include <optional>
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
#include "middleend/mlir/dialect/var/def.h"
#include "middleend/mlir/dialect/var/getptr.h"
#include "middleend/mlir/dialect/var/getvalue.h"
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

using Element = std::optional<int32_t>;
using State = std::vector<Element>;
using States = std::unordered_map<mlir::Value *, State>;

auto TraceCopies(mlir::Value *value) -> mlir::Value * {
  while (value != nullptr) {
    auto *copy = mlir::DynamicOrNull<mlir::arithm::CopyOp>(value->op_);
    if (copy == nullptr) break;
    value = copy->GetValue();
  }
  return value;
}

auto ConstantInt(mlir::Value *value) -> std::optional<int32_t> {
  value = TraceCopies(value);
  auto *constant = value == nullptr ? nullptr : mlir::DynamicOrNull<mlir::arithm::ConstOp>(value->op_);
  if (constant == nullptr || !std::holds_alternative<int>(constant->GetValue())) return std::nullopt;
  return static_cast<int32_t>(std::get<int>(constant->GetValue()));
}

auto EvaluateCondition(mlir::Value *value) -> std::optional<bool> {
  value = TraceCopies(value);
  if (auto constant = ConstantInt(value)) return *constant != 0;
  if (value == nullptr || value->op_ == nullptr) return std::nullopt;
  auto evaluate = [](auto *operation) -> std::optional<std::pair<int32_t, int32_t>> {
    if (operation == nullptr) return std::nullopt;
    auto lhs = ConstantInt(operation->GetLHS());
    auto rhs = ConstantInt(operation->GetRHS());
    if (!lhs || !rhs) return std::nullopt;
    return std::pair<int32_t, int32_t>{*lhs, *rhs};
  };
#define EVALUATE_COMPARE(OP, EXPR)                                                       \
  if (auto operands = evaluate(mlir::DynamicOrNull<mlir::arithm::OP##Op>(value->op_))) { \
    return operands->first EXPR operands->second;                                        \
  }
  EVALUATE_COMPARE(Equal, ==)
  EVALUATE_COMPARE(NotEqual, !=)
  EVALUATE_COMPARE(Less, <)
  EVALUATE_COMPARE(LessEqual, <=)
  EVALUATE_COMPARE(Greater, >)
  EVALUATE_COMPARE(GreaterEqual, >=)
#undef EVALUATE_COMPARE
  return std::nullopt;
}

struct Access {
  mlir::Value *root{nullptr};
  std::vector<mlir::Value *> indices;
};

auto ReadAccess(mlir::Value *pointer) -> Access {
  pointer = TraceCopies(pointer);
  auto *getptr = pointer == nullptr ? nullptr : mlir::DynamicOrNull<mlir::var::VarGetPtrOp>(pointer->op_);
  if (getptr == nullptr) return {pointer, {}};
  return {TraceCopies(getptr->GetPtr()), getptr->GetIndices()};
}

auto TouchesRoot(mlir::Operation *operation, mlir::Value *root) -> bool {
  for (auto *operand : operation->operands_) {
    if (ReadAccess(operand).root == root) return true;
  }
  for (auto *region : operation->regions_) {
    for (auto *block : region->blocks_) {
      for (auto *nested : block->operations_) {
        if (TouchesRoot(nested, root)) return true;
      }
    }
  }
  return false;
}

auto SingleBlock(mlir::Region *region) -> mlir::Block * {
  return region != nullptr && region->blocks_.size() == 1 ? region->blocks_[0] : nullptr;
}

auto SingleYield(mlir::Region *region) -> mlir::scf::YieldOp * {
  auto *block = SingleBlock(region);
  return block == nullptr || block->operations_.empty()
             ? nullptr
             : mlir::DynamicOrNull<mlir::scf::YieldOp>(block->operations_.back());
}

auto ConstantFill(mlir::scf::WhileOp *loop, mlir::Value *root, size_t extent, int32_t *value) -> bool {
  auto induction = loopiv::Recognize(loop);
  if (!induction.valid_ || induction.kind_ != loopiv::CmpKind::kLT || induction.step_kind_ != loopiv::StepKind::kAdd ||
      induction.step_ != 1 ||
      loopiv::ConstantTripCount(induction, static_cast<int64_t>(extent)) != static_cast<int64_t>(extent)) {
    return false;
  }
  auto init = ConstantInt(induction.init_);
  auto *body = SingleBlock(loop->GetBodyRegion());
  if (!init || *init != 0 || body == nullptr) return false;
  auto *iv = body->arguments_[induction.index_];
  mlir::var::VarSetOp *fill = nullptr;
  for (auto *operation : body->operations_) {
    if (auto *load = mlir::DynamicOrNull<mlir::var::VarGetValOp>(operation)) {
      if (ReadAccess(load->GetPtr()).root == root) return false;
    }
    if (auto *store = mlir::DynamicOrNull<mlir::var::VarSetOp>(operation)) {
      auto access = ReadAccess(store->GetPtr());
      if (access.root != root) continue;
      if (fill != nullptr || access.indices.size() != 1 || TraceCopies(access.indices[0]) != TraceCopies(iv)) {
        return false;
      }
      fill = store;
    }
    if (mlir::DynamicOrNull<mlir::func::FuncCallOp>(operation) != nullptr) return false;
  }
  auto constant = fill == nullptr ? std::nullopt : ConstantInt(fill->GetNewValue());
  if (!constant) return false;
  *value = *constant;
  return true;
}

auto ApplyKnownStores(mlir::Block *block, States *states, mlir::scf::WhileOp *stop) -> bool {
  if (block == nullptr) return false;
  for (auto *operation : block->operations_) {
    if (operation == stop) return true;
    if (auto *if_op = mlir::DynamicOrNull<mlir::scf::IfOp>(operation)) {
      auto condition = EvaluateCondition(if_op->GetCondition());
      if (!condition) return false;
      auto *selected = SingleBlock(*condition ? if_op->GetThenRegion() : if_op->GetElseRegion());
      if (!ApplyKnownStores(selected, states, nullptr)) return false;
      continue;
    }
    if (auto *store = mlir::DynamicOrNull<mlir::var::VarSetOp>(operation)) {
      auto access = ReadAccess(store->GetPtr());
      auto found = states->find(access.root);
      if (found == states->end()) continue;
      if (access.indices.size() != 1) return false;
      auto index = ConstantInt(access.indices[0]);
      auto value = ConstantInt(store->GetNewValue());
      if (!index || !value || *index < 0 || static_cast<size_t>(*index) >= found->second.size()) return false;
      found->second[static_cast<size_t>(*index)] = *value;
      continue;
    }
    if (auto *load = mlir::DynamicOrNull<mlir::var::VarGetValOp>(operation)) {
      if (states->count(ReadAccess(load->GetPtr()).root) != 0) return false;
    }
    if (mlir::DynamicOrNull<mlir::func::FuncCallOp>(operation) != nullptr) {
      for (auto &entry : *states) {
        for (auto *operand : operation->operands_) {
          if (ReadAccess(operand).root == entry.first) return false;
        }
      }
    }
    if (mlir::DynamicOrNull<mlir::scf::WhileOp>(operation) != nullptr) return false;
  }
  return stop == nullptr;
}

struct Reduction {
  size_t accumulator_index{0};
  mlir::Value *initial{nullptr};
  mlir::Value *root{nullptr};
};

auto FullSumReduction(mlir::scf::WhileOp *loop, const States &states, Reduction *reduction) -> bool {
  auto induction = loopiv::Recognize(loop);
  auto init = ConstantInt(induction.init_);
  auto *body = SingleBlock(loop->GetBodyRegion());
  auto *condition = SingleBlock(loop->GetCondRegion());
  auto *yield = SingleYield(loop->GetBodyRegion());
  auto *condition_op = condition == nullptr || condition->operations_.empty()
                           ? nullptr
                           : mlir::DynamicOrNull<mlir::scf::ConditionOp>(condition->operations_.back());
  if (!induction.valid_ || induction.kind_ != loopiv::CmpKind::kLT || induction.step_kind_ != loopiv::StepKind::kAdd ||
      induction.step_ != 1 || !init || *init != 0 || body == nullptr || condition_op == nullptr || yield == nullptr) {
    return false;
  }
  auto originals = condition_op->GetOriginalValues();
  if (originals.size() != body->arguments_.size() || originals.size() != yield->GetOperands().size()) return false;

  mlir::var::VarGetValOp *load = nullptr;
  for (auto *operation : body->operations_) {
    if (auto *candidate = mlir::DynamicOrNull<mlir::var::VarGetValOp>(operation)) {
      if (load != nullptr) return false;
      load = candidate;
    }
    if (mlir::DynamicOrNull<mlir::var::VarSetOp>(operation) != nullptr ||
        mlir::DynamicOrNull<mlir::func::FuncCallOp>(operation) != nullptr ||
        (mlir::DynamicOrNull<mlir::scf::WhileOp>(operation) != nullptr && operation != loop)) {
      return false;
    }
  }
  if (load == nullptr) return false;
  auto access = ReadAccess(load->GetPtr());
  auto state = states.find(access.root);
  if (state == states.end() || access.indices.size() != 1 ||
      TraceCopies(access.indices[0]) != TraceCopies(body->arguments_[induction.index_]) ||
      loopiv::ConstantTripCount(induction, static_cast<int64_t>(state->second.size())) !=
          static_cast<int64_t>(state->second.size()) ||
      std::any_of(state->second.begin(), state->second.end(), [](const Element &element) { return !element; })) {
    return false;
  }

  for (size_t index = 0; index < body->arguments_.size(); ++index) {
    if (index == induction.index_) continue;
    auto *next = TraceCopies(yield->GetOperands()[index]);
    auto *add = next == nullptr ? nullptr : mlir::DynamicOrNull<mlir::arithm::AddOp>(next->op_);
    if (add == nullptr) continue;
    auto *argument = TraceCopies(body->arguments_[index]);
    auto *loaded = TraceCopies(load->GetRes());
    bool matches = (TraceCopies(add->GetLHS()) == argument && TraceCopies(add->GetRHS()) == loaded) ||
                   (TraceCopies(add->GetRHS()) == argument && TraceCopies(add->GetLHS()) == loaded);
    if (!matches) continue;
    reduction->accumulator_index = index;
    reduction->initial = originals[index];
    reduction->root = access.root;
    return true;
  }
  return false;
}

auto MakeYieldRegion(context::MLIRContext *context, mlir::Value *value) -> mlir::Region * {
  auto *region = new mlir::Region();
  auto *block = new mlir::Block();
  block->operations_.emplace_back(context->CreateOperation<mlir::scf::YieldOp>(context, value));
  region->blocks_.emplace_back(block);
  return region;
}

auto EffectsArePrivate(mlir::Operation *operation, mlir::Value *root) -> bool {
  if (mlir::DynamicOrNull<mlir::func::FuncCallOp>(operation) != nullptr ||
      mlir::DynamicOrNull<mlir::scf::BreakOp>(operation) != nullptr ||
      mlir::DynamicOrNull<mlir::scf::ContinueOp>(operation) != nullptr) {
    return false;
  }
  if (auto *load = mlir::DynamicOrNull<mlir::var::VarGetValOp>(operation)) {
    if (ReadAccess(load->GetPtr()).root != root) return false;
  }
  if (auto *store = mlir::DynamicOrNull<mlir::var::VarSetOp>(operation)) {
    if (ReadAccess(store->GetPtr()).root != root) return false;
  }
  for (auto *region : operation->regions_) {
    for (auto *block : region->blocks_) {
      for (auto *nested : block->operations_) {
        if (!EffectsArePrivate(nested, root)) return false;
      }
    }
  }
  return true;
}

auto RootDoesNotEscape(mlir::Value *root) -> bool {
  for (auto *use : root->GetUses()) {
    auto *getptr = mlir::DynamicOrNull<mlir::var::VarGetPtrOp>(use->operation_);
    if (getptr == nullptr) return false;
    for (auto *pointer_use : getptr->GetRes()->GetUses()) {
      if (mlir::DynamicOrNull<mlir::var::VarGetValOp>(pointer_use->operation_) == nullptr &&
          mlir::DynamicOrNull<mlir::var::VarSetOp>(pointer_use->operation_) == nullptr) {
        return false;
      }
    }
  }
  return true;
}

struct ModularRecurrence {
  size_t accumulator_index{0};
  mlir::Value *bound{nullptr};
  mlir::Value *initial{nullptr};
  mlir::arithm::LessOp *condition_compare{nullptr};
  int32_t delta{0};
  int32_t modulus{0};
};

auto FindModularRecurrence(mlir::scf::WhileOp *loop, mlir::Value *private_root, ModularRecurrence *recurrence) -> bool {
  auto induction = loopiv::Recognize(loop);
  auto *condition_block = SingleBlock(loop->GetCondRegion());
  auto *body = SingleBlock(loop->GetBodyRegion());
  auto *condition = condition_block == nullptr || condition_block->operations_.empty()
                        ? nullptr
                        : mlir::DynamicOrNull<mlir::scf::ConditionOp>(condition_block->operations_.back());
  auto *yield = SingleYield(loop->GetBodyRegion());
  auto original = condition == nullptr ? std::vector<mlir::Value *>{} : condition->GetOriginalValues();
  auto iv_init = ConstantInt(induction.init_);
  if (!induction.valid_ || induction.kind_ != loopiv::CmpKind::kLT || induction.step_kind_ != loopiv::StepKind::kAdd ||
      induction.step_ != 1 || !iv_init || *iv_init != 0 || body == nullptr || yield == nullptr ||
      original.size() != body->arguments_.size() || original.size() != yield->GetOperands().size() ||
      !RootDoesNotEscape(private_root)) {
    return false;
  }
  for (auto *operation : body->operations_) {
    if (!EffectsArePrivate(operation, private_root)) return false;
  }
  for (size_t index = 0; index < original.size(); ++index) {
    if (index == induction.index_) continue;
    auto *next = TraceCopies(yield->GetOperands()[index]);
    auto *mod = next == nullptr ? nullptr : mlir::DynamicOrNull<mlir::arithm::ModOp>(next->op_);
    auto modulus = mod == nullptr ? std::nullopt : ConstantInt(mod->GetRHS());
    auto *add_value = mod == nullptr ? nullptr : TraceCopies(mod->GetLHS());
    auto *add = add_value == nullptr ? nullptr : mlir::DynamicOrNull<mlir::arithm::AddOp>(add_value->op_);
    if (add == nullptr || !modulus) continue;
    auto *argument = TraceCopies(body->arguments_[index]);
    std::optional<int32_t> delta;
    if (TraceCopies(add->GetLHS()) == argument) delta = ConstantInt(add->GetRHS());
    if (TraceCopies(add->GetRHS()) == argument) delta = ConstantInt(add->GetLHS());
    auto initial = ConstantInt(original[index]);
    if (!delta || !initial || *modulus <= 0 || *delta <= 0 || *delta >= *modulus || *initial < 0 ||
        *initial >= *modulus || static_cast<int64_t>(*modulus - 1) + *delta > INT32_MAX) {
      continue;
    }
    int64_t period = *modulus / std::gcd(*modulus, *delta);
    if ((period - 1) * *delta + *initial > INT32_MAX) continue;
    for (size_t result = 0; result < loop->results_.size(); ++result) {
      if (result != index && !loop->GetResultByIndex(result)->GetUses().empty()) return false;
    }
    auto *compare_value = TraceCopies(condition->GetCondition());
    auto *compare = compare_value == nullptr ? nullptr : mlir::DynamicOrNull<mlir::arithm::LessOp>(compare_value->op_);
    if (compare == nullptr) return false;
    recurrence->accumulator_index = index;
    recurrence->bound = induction.bound_;
    recurrence->initial = original[index];
    recurrence->condition_compare = compare;
    recurrence->delta = *delta;
    recurrence->modulus = *modulus;
    return true;
  }
  return false;
}

auto CloseModularRecurrence(context::MLIRContext *context, mlir::Block *parent, size_t loop_index,
                            mlir::scf::WhileOp *loop, mlir::Value *private_root) -> bool {
  ModularRecurrence recurrence;
  if (!FindModularRecurrence(loop, private_root, &recurrence)) return false;
  auto *integer_type = recurrence.bound->GetType();
  auto *bool_type = recurrence.condition_compare->GetRes()->GetType();
  int32_t period = recurrence.modulus / std::gcd(recurrence.modulus, recurrence.delta);

  auto *zero = context->CreateOperation<mlir::arithm::ConstOp>(context, 0);
  auto *period_value = context->CreateOperation<mlir::arithm::ConstOp>(context, period);
  auto *delta = context->CreateOperation<mlir::arithm::ConstOp>(context, recurrence.delta);
  auto *modulus = context->CreateOperation<mlir::arithm::ConstOp>(context, recurrence.modulus);
  for (auto *value : {zero->GetRes(), period_value->GetRes(), delta->GetRes(), modulus->GetRes()}) {
    value->SetType(integer_type);
  }
  auto *positive = context->CreateOperation<mlir::arithm::LessOp>(context, zero->GetRes(), recurrence.bound);
  positive->GetRes()->SetType(bool_type);
  auto *trip_count = context->CreateOperation<mlir::scf::IfOp>(context, 1, positive->GetRes(),
                                                               MakeYieldRegion(context, recurrence.bound),
                                                               MakeYieldRegion(context, zero->GetRes()));
  trip_count->GetResultByIndex(0)->SetType(integer_type);
  std::vector<mlir::Operation *> before = {zero, period_value, delta, modulus, positive, trip_count};
  parent->operations_.insert(parent->operations_.begin() + static_cast<std::ptrdiff_t>(loop_index), before.begin(),
                             before.end());
  recurrence.condition_compare->operands_[1] = zero->GetRes();

  auto *reduced =
      context->CreateOperation<mlir::arithm::ModOp>(context, trip_count->GetResultByIndex(0), period_value->GetRes());
  auto *scaled = context->CreateOperation<mlir::arithm::MulOp>(context, reduced->GetRes(), delta->GetRes());
  auto *sum = context->CreateOperation<mlir::arithm::AddOp>(context, recurrence.initial, scaled->GetRes());
  auto *final = context->CreateOperation<mlir::arithm::ModOp>(context, sum->GetRes(), modulus->GetRes());
  for (auto *value : {reduced->GetRes(), scaled->GetRes(), sum->GetRes(), final->GetRes()}) {
    value->SetType(integer_type);
  }
  size_t after = loop_index + before.size() + 1;
  parent->operations_.insert(parent->operations_.begin() + static_cast<std::ptrdiff_t>(after),
                             {reduced, scaled, sum, final});
  loop->GetResultByIndex(recurrence.accumulator_index)->ReplaceAllUsesWith(final->GetRes());
  return true;
}

auto FoldReductionsInLoop(context::MLIRContext *context, mlir::Block *parent, size_t outer_index,
                          mlir::scf::WhileOp *outer, const States &incoming, size_t *reductions_seen, size_t *folded,
                          size_t *recurrences_closed) -> bool {
  auto *body = SingleBlock(outer->GetBodyRegion());
  if (body == nullptr) return false;
  States state = incoming;
  for (size_t index = 0; index < body->operations_.size(); ++index) {
    auto *inner = mlir::DynamicOrNull<mlir::scf::WhileOp>(body->operations_[index]);
    if (inner == nullptr) continue;
    if (!ApplyKnownStores(body, &state, inner)) return false;
    Reduction reduction;
    if (FullSumReduction(inner, state, &reduction)) {
      ++*reductions_seen;
      uint32_t sum = 0;
      for (const auto &element : state.at(reduction.root)) sum += static_cast<uint32_t>(*element);
      auto *constant = context->CreateOperation<mlir::arithm::ConstOp>(context, static_cast<int32_t>(sum));
      constant->GetRes()->SetType(inner->GetResultByIndex(reduction.accumulator_index)->GetType());
      auto *add = context->CreateOperation<mlir::arithm::AddOp>(context, reduction.initial, constant->GetRes());
      add->GetRes()->SetType(inner->GetResultByIndex(reduction.accumulator_index)->GetType());
      body->operations_.insert(body->operations_.begin() + static_cast<std::ptrdiff_t>(index), constant);
      body->operations_.insert(body->operations_.begin() + static_cast<std::ptrdiff_t>(index + 1), add);
      inner->GetResultByIndex(reduction.accumulator_index)->ReplaceAllUsesWith(add->GetRes());
      index += 2;
      ++*folded;
      if (CloseModularRecurrence(context, parent, outer_index, outer, reduction.root)) ++*recurrences_closed;
      return true;
    }
    return false;
  }
  return false;
}

}  // namespace

auto MLIRConstantArrayReductionPass::Run() -> void {
  PreRun();
  arrays_seen_ = 0;
  reductions_seen_ = 0;
  folded_ = 0;
  recurrences_closed_ = 0;
  if (std::getenv("CONSTANT_ARRAY_REDUCTION_OFF") == nullptr) {
    auto *module = GET_MLIR_CONTEXT()->GetModuleOp();
    if (module == nullptr) throw std::logic_error("Module operation is not set in MLIRContext");
    for (auto *operation : module->GetBlock()->operations_) {
      if (auto *function = mlir::DynamicOrNull<mlir::func::FuncDefOp>(operation)) RunOnFunction(function);
    }
  }
  if (std::getenv("LOOP_OPT_STATS") != nullptr || std::getenv("CONSTANT_ARRAY_REDUCTION_STATS") != nullptr) {
    std::cerr << "ConstantArrayReduction: arrays=" << arrays_seen_ << " reductions=" << reductions_seen_
              << " folded=" << folded_ << " recurrences=" << recurrences_closed_ << '\n';
  }
}

auto MLIRConstantArrayReductionPass::RunOnFunction(mlir::func::FuncDefOp *function) -> bool {
  mlir::utils::RebuildFunctionUses(function);
  auto *block = function->GetEntryBlock();
  States states;
  bool changed = false;
  for (size_t operation_index = 0; operation_index < block->operations_.size(); ++operation_index) {
    auto *operation = block->operations_[operation_index];
    if (auto *definition = mlir::DynamicOrNull<mlir::var::VarDefOp>(operation)) {
      const auto &dimensions = definition->GetDimensions();
      if (dimensions.size() == 1 && dimensions[0] > 0 && definition->GetRes()->GetType() != nullptr &&
          definition->GetRes()->GetType()->GetKind() == type::TypeKind::ARRAY) {
        states.emplace(definition->GetRes(), State(static_cast<size_t>(dimensions[0])));
        ++arrays_seen_;
      }
      continue;
    }
    auto *loop = mlir::DynamicOrNull<mlir::scf::WhileOp>(operation);
    if (loop == nullptr) {
      for (auto iter = states.begin(); iter != states.end();) {
        if (TouchesRoot(operation, iter->first)) {
          iter = states.erase(iter);
        } else {
          ++iter;
        }
      }
      continue;
    }
    bool filled = false;
    std::vector<mlir::Value *> filled_roots;
    for (auto &entry : states) {
      int32_t value = 0;
      if (ConstantFill(loop, entry.first, entry.second.size(), &value)) {
        std::fill(entry.second.begin(), entry.second.end(), Element(value));
        filled = true;
        filled_roots.emplace_back(entry.first);
      }
    }
    if (filled) {
      for (auto iter = states.begin(); iter != states.end();) {
        bool is_fill = std::find(filled_roots.begin(), filled_roots.end(), iter->first) != filled_roots.end();
        if (!is_fill && TouchesRoot(loop, iter->first)) {
          iter = states.erase(iter);
        } else {
          ++iter;
        }
      }
      continue;
    }
    changed = FoldReductionsInLoop(GET_MLIR_CONTEXT(), block, operation_index, loop, states, &reductions_seen_,
                                   &folded_, &recurrences_closed_) ||
              changed;
    if (changed) break;
    for (auto iter = states.begin(); iter != states.end();) {
      if (TouchesRoot(loop, iter->first)) {
        iter = states.erase(iter);
      } else {
        ++iter;
      }
    }
  }
  if (changed) mlir::utils::RebuildFunctionUses(function);
  return changed;
}

}  // namespace middleend::pass::transform
