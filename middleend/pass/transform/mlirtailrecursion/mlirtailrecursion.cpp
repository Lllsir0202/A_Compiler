#include "middleend/pass/transform/mlirtailrecursion/mlirtailrecursion.h"
#include <vector>
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/arithm/binaryop.h"
#include "middleend/mlir/dialect/arithm/copy.h"
#include "middleend/mlir/dialect/arithm/unaryop.h"
#include "middleend/mlir/dialect/func/funccall.h"
#include "middleend/mlir/dialect/func/funcdef.h"
#include "middleend/mlir/dialect/func/return.h"
#include "middleend/mlir/dialect/scf/condition.h"
#include "middleend/mlir/dialect/scf/if.h"
#include "middleend/mlir/dialect/scf/while.h"
#include "middleend/mlir/dialect/scf/yield.h"
#include "middleend/mlir/dialect/var/getptr.h"
#include "middleend/mlir/dialect/var/getvalue.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/block.h"
#include "middleend/mlir/utils/struct/region.h"
#include "middleend/mlir/utils/value/value.h"
#include "utils/type/type.h"

#define GET_MLIR_CONTEXT() compiler_context_->GetIRContext()->GetMLIRContext()
#define GET_TYPE_CONTEXT() compiler_context_->GetTypeContext()

namespace middleend::pass::transform {
auto MLIRTailRecursionEliminatePass::Run() -> void {
  bool changed = false;
  auto *module_op = GET_MLIR_CONTEXT()->GetModuleOp();
  if (module_op == nullptr) {
    throw std::logic_error("Module operation is not set in MLIRContext");
  }

  for (auto *operation : module_op->GetBlock()->operations_) {
    auto *func_op = mlir::DynamicOrNull<mlir::func::FuncDefOp>(operation);
    if (func_op == nullptr) {
      continue;
    }
    changed |= RunOnFunction(func_op);
  }

  if (!changed) {
    return;
  }
  for (auto *operation : module_op->GetBlock()->operations_) {
    auto *func_op = mlir::DynamicOrNull<mlir::func::FuncDefOp>(operation);
    if (func_op != nullptr) {
      RebuildUses(func_op);
    }
  }
}

auto MLIRTailRecursionEliminatePass::RunOnFunction(mlir::func::FuncDefOp *func_op) -> bool {
  TailRecursionPattern pattern;
  if (MatchTailRecursion(func_op, &pattern)) {
    return RewriteTailRecursion(func_op, pattern);
  }
  return RewriteMultiExitTailRecursion(func_op);
}

auto MLIRTailRecursionEliminatePass::IsFallThroughRegion(mlir::Region *region) -> bool {
  if (region == nullptr || region->blocks_.size() != 1 || region->blocks_[0]->operations_.size() != 1) {
    return false;
  }
  auto *yield_op = mlir::DynamicOrNull<mlir::scf::YieldOp>(region->blocks_[0]->operations_[0]);
  return yield_op != nullptr && yield_op->GetOperands().empty();
}

auto MLIRTailRecursionEliminatePass::IsTerminalTailBlock(mlir::Block *block, mlir::func::FuncDefOp *func_op,
                                                         bool *saw_tail, size_t start) -> bool {
  for (size_t index = start; index < block->operations_.size(); ++index) {
    auto *operation = block->operations_[index];
    if (auto *call = mlir::DynamicOrNull<mlir::func::FuncCallOp>(operation)) {
      if (call->GetName() != func_op->GetName() || index + 1 >= block->operations_.size()) {
        return false;
      }
      auto *return_op = mlir::DynamicOrNull<mlir::func::ReturnOp>(block->operations_[index + 1]);
      auto *yield_op = mlir::DynamicOrNull<mlir::scf::YieldOp>(block->operations_[index + 1]);
      bool returns_call = return_op != nullptr && return_op->GetReturnValue() == call->GetRes();
      bool yields_call =
          yield_op != nullptr && yield_op->GetOperands().size() == 1 && yield_op->GetOperands()[0] == call->GetRes();
      if ((!returns_call && !yields_call) || call->GetOperands().size() != func_op->GetArguments().size()) {
        return false;
      }
      *saw_tail = true;
      return true;
    }
    if (auto *return_op = mlir::DynamicOrNull<mlir::func::ReturnOp>(operation)) {
      return return_op->GetReturnValue() != nullptr;
    }
    if (auto *if_op = mlir::DynamicOrNull<mlir::scf::IfOp>(operation)) {
      if (if_op->GetThenRegion() == nullptr || if_op->GetElseRegion() == nullptr ||
          if_op->GetThenRegion()->blocks_.size() != 1 || if_op->GetElseRegion()->blocks_.size() != 1 ||
          if_op->results_.size() > 1) {
        return false;
      }
      if (if_op->results_.size() == 1) {
        if (index + 1 >= block->operations_.size()) {
          return false;
        }
        auto *return_op = mlir::DynamicOrNull<mlir::func::ReturnOp>(block->operations_[index + 1]);
        auto *yield_op = mlir::DynamicOrNull<mlir::scf::YieldOp>(block->operations_[index + 1]);
        bool returns_if = return_op != nullptr && return_op->GetReturnValue() == if_op->GetResultByIndex(0);
        bool yields_if = yield_op != nullptr && yield_op->GetOperands().size() == 1 &&
                         yield_op->GetOperands()[0] == if_op->GetResultByIndex(0);
        if (!returns_if && !yields_if) {
          return false;
        }
      }
      // An `if` with no `else` does not end control flow: whichever branch is empty continues with
      // the operations that follow this `if`, so scan those as that branch's continuation.
      bool then_falls = IsFallThroughRegion(if_op->GetThenRegion());
      bool else_falls = IsFallThroughRegion(if_op->GetElseRegion());
      if (then_falls && else_falls) {
        continue;  // an empty `if` decides nothing; keep scanning this block
      }
      if (then_falls || else_falls) {
        if (if_op->results_.size() != 0) {
          return false;  // a value-producing if cannot fall through
        }
        auto *terminal = then_falls ? if_op->GetElseRegion() : if_op->GetThenRegion();
        return IsTerminalTailBlock(terminal->blocks_[0], func_op, saw_tail) &&
               IsTerminalTailBlock(block, func_op, saw_tail, index + 1);
      }
      return IsTerminalTailBlock(if_op->GetThenRegion()->blocks_[0], func_op, saw_tail) &&
             IsTerminalTailBlock(if_op->GetElseRegion()->blocks_[0], func_op, saw_tail);
    }
    if (auto *yield_op = mlir::DynamicOrNull<mlir::scf::YieldOp>(operation)) {
      return yield_op->GetOperands().size() == 1;
    }
    if (!CanCloneOperation(operation)) {
      return false;
    }
  }
  return false;
}

auto MLIRTailRecursionEliminatePass::BuildTailState(mlir::Block *source, mlir::Block *destination,
                                                    mlir::func::FuncDefOp *func_op,
                                                    const std::vector<mlir::Value *> &current_args,
                                                    mlir::Value *current_result,
                                                    size_t start) -> std::vector<mlir::Value *> {
  auto make_state = [&](const std::vector<mlir::Value *> &args, mlir::Value *active, mlir::Value *result) {
    std::vector<mlir::Value *> state = args;
    state.emplace_back(active);
    state.emplace_back(result);
    return state;
  };

  for (size_t index = start; index < source->operations_.size(); ++index) {
    auto *operation = source->operations_[index];
    if (auto *call = mlir::DynamicOrNull<mlir::func::FuncCallOp>(operation)) {
      std::vector<mlir::Value *> next_args;
      next_args.reserve(call->GetOperands().size());
      for (auto *operand : call->GetOperands()) {
        next_args.emplace_back(RemapValue(operand));
      }
      return make_state(next_args, tail_continue_, current_result);
    }
    if (auto *return_op = mlir::DynamicOrNull<mlir::func::ReturnOp>(operation)) {
      return make_state(current_args, tail_stop_, RemapValue(return_op->GetReturnValue()));
    }
    if (auto *yield_op = mlir::DynamicOrNull<mlir::scf::YieldOp>(operation)) {
      return make_state(current_args, tail_stop_, RemapValue(yield_op->GetOperands()[0]));
    }
    if (auto *if_op = mlir::DynamicOrNull<mlir::scf::IfOp>(operation)) {
      bool then_falls = IsFallThroughRegion(if_op->GetThenRegion());
      bool else_falls = IsFallThroughRegion(if_op->GetElseRegion());
      if (then_falls && else_falls) {
        continue;
      }
      auto outer_map = value_map_;
      auto *then_region = new mlir::Region();
      auto *then_block = new mlir::Block();
      then_region->blocks_.emplace_back(then_block);
      // A fall-through branch continues with the operations after this `if`, so its state is built
      // from the rest of the enclosing block rather than from the (empty) region.
      auto then_state = then_falls
                            ? BuildTailState(source, then_block, func_op, current_args, current_result, index + 1)
                            : BuildTailState(if_op->GetThenRegion()->blocks_[0], then_block, func_op, current_args,
                                             current_result);
      then_block->AddOperation(GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::YieldOp>(GET_MLIR_CONTEXT(), then_state));

      value_map_ = outer_map;
      auto *else_region = new mlir::Region();
      auto *else_block = new mlir::Block();
      else_region->blocks_.emplace_back(else_block);
      auto else_state = else_falls
                            ? BuildTailState(source, else_block, func_op, current_args, current_result, index + 1)
                            : BuildTailState(if_op->GetElseRegion()->blocks_[0], else_block, func_op, current_args,
                                             current_result);
      else_block->AddOperation(GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::YieldOp>(GET_MLIR_CONTEXT(), else_state));
      value_map_ = outer_map;

      auto *state_if = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::IfOp>(
          GET_MLIR_CONTEXT(), then_state.size(), RemapValue(if_op->GetCondition()), then_region, else_region);
      for (size_t state_index = 0; state_index < then_state.size(); ++state_index) {
        state_if->GetResultByIndex(state_index)->SetType(then_state[state_index]->GetType());
      }
      destination->AddOperation(state_if);
      std::vector<mlir::Value *> results;
      results.reserve(then_state.size());
      for (size_t state_index = 0; state_index < then_state.size(); ++state_index) {
        results.emplace_back(state_if->GetResultByIndex(state_index));
      }
      return results;
    }
    destination->AddOperation(CloneOperation(operation));
  }
  throw std::logic_error("tail-recursive terminal block did not produce a state");
}

auto MLIRTailRecursionEliminatePass::RewriteMultiExitTailRecursion(mlir::func::FuncDefOp *func_op) -> bool {
  if (func_op->GetReturnType() == nullptr || func_op->GetReturnType()->GetKind() != type::TypeKind::INT) {
    return false;
  }
  bool saw_tail = false;
  if (!IsTerminalTailBlock(func_op->GetEntryBlock(), func_op, &saw_tail) || !saw_tail) {
    return false;
  }

  auto arguments = func_op->GetArguments();
  auto *one = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::ConstOp>(GET_MLIR_CONTEXT(), 1);
  one->GetRes()->SetType(GET_TYPE_CONTEXT()->GetIntegerType());
  auto *active = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::CastOp>(
      GET_MLIR_CONTEXT(), one->GetRes(), GET_TYPE_CONTEXT()->GetIntegerType(), GET_TYPE_CONTEXT()->GetBooleanType());
  auto *zero = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::ConstOp>(GET_MLIR_CONTEXT(), 0);
  zero->GetRes()->SetType(GET_TYPE_CONTEXT()->GetIntegerType());
  auto *stopped = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::CastOp>(
      GET_MLIR_CONTEXT(), zero->GetRes(), GET_TYPE_CONTEXT()->GetIntegerType(), GET_TYPE_CONTEXT()->GetBooleanType());
  tail_continue_ = active->GetRes();
  tail_stop_ = stopped->GetRes();

  auto *cond_region = new mlir::Region();
  auto *cond_block = new mlir::Block();
  cond_region->blocks_.emplace_back(cond_block);
  auto *body_region = new mlir::Region();
  auto *body_block = new mlir::Block();
  body_region->blocks_.emplace_back(body_block);

  std::vector<mlir::Value *> loop_args;
  std::vector<mlir::Value *> initial_values = arguments;
  initial_values.emplace_back(tail_continue_);
  initial_values.emplace_back(zero->GetRes());
  loop_args.reserve(initial_values.size());
  for (auto *initial : initial_values) {
    auto *loop_arg = CloneValue(initial);
    cond_block->AddArgument(loop_arg);
    body_block->AddArgument(loop_arg);
    loop_args.emplace_back(loop_arg);
  }
  auto *condition = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::ConditionOp>(
      GET_MLIR_CONTEXT(), loop_args[arguments.size()], std::unordered_map<symboltable::VarEntry *, mlir::Value *>{});
  condition->SetLoopCarriedValues(loop_args, initial_values);
  cond_block->AddOperation(condition);

  value_map_.clear();
  std::vector<mlir::Value *> current_args;
  for (size_t index = 0; index < arguments.size(); ++index) {
    value_map_[arguments[index]] = loop_args[index];
    current_args.emplace_back(loop_args[index]);
  }
  auto next_state = BuildTailState(func_op->GetEntryBlock(), body_block, func_op, current_args, loop_args.back());
  body_block->AddOperation(GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::YieldOp>(GET_MLIR_CONTEXT(), next_state));

  auto *while_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::WhileOp>(GET_MLIR_CONTEXT(), initial_values.size(),
                                                                           cond_region, body_region);
  for (size_t index = 0; index < initial_values.size(); ++index) {
    while_op->GetResultByIndex(index)->SetType(initial_values[index]->GetType());
  }
  auto *return_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::func::ReturnOp>(
      GET_MLIR_CONTEXT(), func_op->GetReturnType(), while_op->GetResultByIndex(initial_values.size() - 1));
  func_op->GetEntryBlock()->operations_ = {one, active, zero, stopped, while_op, return_op};
  return true;
}

auto MLIRTailRecursionEliminatePass::MatchTailRecursion(mlir::func::FuncDefOp *func_op,
                                                        TailRecursionPattern *pattern) -> bool {
  auto *block = func_op->GetEntryBlock();
  if (block->operations_.size() < 2) {
    return false;
  }
  size_t if_index = block->operations_.size() - 2;
  auto *if_op = mlir::DynamicOrNull<mlir::scf::IfOp>(block->operations_[if_index]);
  auto *return_op = mlir::DynamicOrNull<mlir::func::ReturnOp>(block->operations_.back());
  if (if_op == nullptr || return_op == nullptr || if_op->results_.size() != 1) {
    return false;
  }
  if (return_op->GetReturnValue() != if_op->GetResultByIndex(0)) {
    return false;
  }
  if (if_op->GetThenRegion()->blocks_.size() != 1 || if_op->GetElseRegion()->blocks_.size() != 1) {
    return false;
  }

  auto *then_block = if_op->GetThenRegion()->blocks_[0];
  auto *else_block = if_op->GetElseRegion()->blocks_[0];
  if (then_block->operations_.empty() || else_block->operations_.size() < 2) {
    return false;
  }
  auto *then_yield = mlir::DynamicOrNull<mlir::scf::YieldOp>(then_block->operations_.back());
  auto *else_yield = mlir::DynamicOrNull<mlir::scf::YieldOp>(else_block->operations_.back());
  if (then_yield == nullptr || else_yield == nullptr || then_yield->GetOperands().size() != 1 ||
      else_yield->GetOperands().size() != 1) {
    return false;
  }

  auto *call_op =
      mlir::DynamicOrNull<mlir::func::FuncCallOp>(else_block->operations_[else_block->operations_.size() - 2]);
  if (call_op == nullptr || call_op->GetName() != func_op->GetName() ||
      else_yield->GetOperands()[0] != call_op->GetRes()) {
    return false;
  }
  if (call_op->GetOperands().size() != func_op->GetArguments().size()) {
    return false;
  }

  for (size_t i = 0; i < if_index; ++i) {
    if (!CanCloneOperation(block->operations_[i])) {
      return false;
    }
  }
  for (auto *operation : then_block->operations_) {
    if (operation == then_yield) {
      continue;
    }
    if (!CanCloneOperation(operation)) {
      return false;
    }
  }
  for (size_t i = 0; i + 2 < else_block->operations_.size(); ++i) {
    if (!CanCloneOperation(else_block->operations_[i])) {
      return false;
    }
  }

  pattern->if_op = if_op;
  pattern->return_op = return_op;
  pattern->call_op = call_op;
  pattern->then_yield = then_yield;
  pattern->else_yield = else_yield;
  pattern->if_index = if_index;
  return true;
}

auto MLIRTailRecursionEliminatePass::RewriteTailRecursion(mlir::func::FuncDefOp *func_op,
                                                          const TailRecursionPattern &pattern) -> bool {
  auto arguments = func_op->GetArguments();
  auto *then_block = pattern.if_op->GetThenRegion()->blocks_[0];
  auto *else_block = pattern.if_op->GetElseRegion()->blocks_[0];

  auto *cond_region = new mlir::Region();
  auto *cond_block = new mlir::Block();
  cond_region->blocks_.emplace_back(cond_block);
  auto *body_region = new mlir::Region();
  auto *body_block = new mlir::Block();
  body_region->blocks_.emplace_back(body_block);

  std::vector<mlir::Value *> loop_args;
  loop_args.reserve(arguments.size());
  for (auto *argument : arguments) {
    auto *loop_arg = CloneValue(argument);
    cond_block->AddArgument(loop_arg);
    body_block->AddArgument(loop_arg);
    loop_args.emplace_back(loop_arg);
  }

  value_map_.clear();
  for (size_t i = 0; i < arguments.size(); ++i) {
    value_map_[arguments[i]] = loop_args[i];
  }
  for (size_t i = 0; i < pattern.if_index; ++i) {
    cond_block->AddOperation(CloneOperation(func_op->GetEntryBlock()->operations_[i]));
  }
  mlir::Value *base_condition = RemapValue(pattern.if_op->GetCondition());
  auto *loop_condition = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::NotOp>(GET_MLIR_CONTEXT(), base_condition);
  loop_condition->GetRes()->SetType(GET_TYPE_CONTEXT()->GetBooleanType());
  cond_block->AddOperation(loop_condition);
  auto *condition = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::ConditionOp>(
      GET_MLIR_CONTEXT(), loop_condition->GetRes(), std::unordered_map<symboltable::VarEntry *, mlir::Value *>{});
  condition->SetLoopCarriedValues(loop_args, arguments);
  cond_block->AddOperation(condition);

  value_map_.clear();
  for (size_t i = 0; i < arguments.size(); ++i) {
    value_map_[arguments[i]] = loop_args[i];
  }
  for (size_t i = 0; i < pattern.if_index; ++i) {
    body_block->AddOperation(CloneOperation(func_op->GetEntryBlock()->operations_[i]));
  }
  for (size_t i = 0; i + 2 < else_block->operations_.size(); ++i) {
    body_block->AddOperation(CloneOperation(else_block->operations_[i]));
  }
  std::vector<mlir::Value *> next_args;
  for (auto *operand : pattern.call_op->GetOperands()) {
    next_args.emplace_back(RemapValue(operand));
  }
  auto *body_yield = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::YieldOp>(GET_MLIR_CONTEXT(), next_args);
  body_block->AddOperation(body_yield);

  auto *while_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::WhileOp>(GET_MLIR_CONTEXT(), arguments.size(),
                                                                           cond_region, body_region);
  for (size_t i = 0; i < arguments.size(); ++i) {
    while_op->GetResultByIndex(i)->SetType(arguments[i]->GetType());
  }

  value_map_.clear();
  for (size_t i = 0; i < arguments.size(); ++i) {
    value_map_[arguments[i]] = while_op->GetResultByIndex(i);
  }
  std::vector<mlir::Operation *> new_operations;
  new_operations.emplace_back(while_op);
  for (size_t i = 0; i < pattern.if_index; ++i) {
    new_operations.emplace_back(CloneOperation(func_op->GetEntryBlock()->operations_[i]));
  }
  for (auto *operation : then_block->operations_) {
    if (operation == pattern.then_yield) {
      continue;
    }
    new_operations.emplace_back(CloneOperation(operation));
  }
  auto *return_value = RemapValue(pattern.then_yield->GetOperands()[0]);
  auto *return_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::func::ReturnOp>(GET_MLIR_CONTEXT(),
                                                                              func_op->GetReturnType(), return_value);
  new_operations.emplace_back(return_op);

  pattern.if_op->regions_.clear();
  func_op->GetEntryBlock()->operations_ = std::move(new_operations);
  return true;
}

auto MLIRTailRecursionEliminatePass::CloneValue(mlir::Value *value) -> mlir::Value * {
  auto *new_value = GET_MLIR_CONTEXT()->CreateValue();
  new_value->SetType(value->GetType());
  new_value->name_ = value->name_;
  return new_value;
}

auto MLIRTailRecursionEliminatePass::RemapValue(mlir::Value *value) -> mlir::Value * {
  auto iter = value_map_.find(value);
  if (iter != value_map_.end()) {
    return iter->second;
  }
  return value;
}

auto MLIRTailRecursionEliminatePass::CanCloneOperation(mlir::Operation *operation) -> bool {
  if (mlir::DynamicOrNull<mlir::func::FuncCallOp>(operation) != nullptr ||
      mlir::DynamicOrNull<mlir::func::ReturnOp>(operation) != nullptr ||
      mlir::DynamicOrNull<mlir::scf::IfOp>(operation) != nullptr ||
      mlir::DynamicOrNull<mlir::scf::WhileOp>(operation) != nullptr ||
      mlir::DynamicOrNull<mlir::scf::YieldOp>(operation) != nullptr ||
      mlir::DynamicOrNull<mlir::scf::ConditionOp>(operation) != nullptr) {
    return false;
  }
  return mlir::DynamicOrNull<mlir::arithm::ConstOp>(operation) != nullptr ||
         mlir::DynamicOrNull<mlir::arithm::CopyOp>(operation) != nullptr ||
         mlir::DynamicOrNull<mlir::arithm::CastOp>(operation) != nullptr ||
         mlir::DynamicOrNull<mlir::arithm::NegOp>(operation) != nullptr ||
         mlir::DynamicOrNull<mlir::arithm::NotOp>(operation) != nullptr ||
         mlir::DynamicOrNull<mlir::arithm::AddOp>(operation) != nullptr ||
         mlir::DynamicOrNull<mlir::arithm::SubOp>(operation) != nullptr ||
         mlir::DynamicOrNull<mlir::arithm::MulOp>(operation) != nullptr ||
         mlir::DynamicOrNull<mlir::arithm::DivOp>(operation) != nullptr ||
         mlir::DynamicOrNull<mlir::arithm::ModOp>(operation) != nullptr ||
         mlir::DynamicOrNull<mlir::arithm::EqualOp>(operation) != nullptr ||
         mlir::DynamicOrNull<mlir::arithm::NotEqualOp>(operation) != nullptr ||
         mlir::DynamicOrNull<mlir::arithm::LessOp>(operation) != nullptr ||
         mlir::DynamicOrNull<mlir::arithm::LessEqualOp>(operation) != nullptr ||
         mlir::DynamicOrNull<mlir::arithm::GreaterOp>(operation) != nullptr ||
         mlir::DynamicOrNull<mlir::arithm::GreaterEqualOp>(operation) != nullptr ||
         mlir::DynamicOrNull<mlir::var::VarGetPtrOp>(operation) != nullptr ||
         mlir::DynamicOrNull<mlir::var::VarGetValOp>(operation) != nullptr;
}

auto MLIRTailRecursionEliminatePass::CloneOperation(mlir::Operation *operation) -> mlir::Operation * {
  if (auto *copy_op = mlir::DynamicOrNull<mlir::arithm::CopyOp>(operation)) {
    auto *new_op =
        GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::CopyOp>(GET_MLIR_CONTEXT(), RemapValue(copy_op->GetValue()));
    new_op->GetRes()->SetType(copy_op->GetRes()->GetType());
    value_map_[copy_op->GetRes()] = new_op->GetRes();
    return new_op;
  }
  if (auto *const_op = mlir::DynamicOrNull<mlir::arithm::ConstOp>(operation)) {
    mlir::arithm::ConstOp *new_op = nullptr;
    auto value = const_op->GetValue();
    if (std::holds_alternative<int>(value)) {
      new_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::ConstOp>(GET_MLIR_CONTEXT(), std::get<int>(value));
    } else {
      new_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::ConstOp>(GET_MLIR_CONTEXT(), std::get<float>(value));
    }
    new_op->GetRes()->SetType(const_op->GetRes()->GetType());
    value_map_[const_op->GetRes()] = new_op->GetRes();
    return new_op;
  }
  if (auto *cast_op = mlir::DynamicOrNull<mlir::arithm::CastOp>(operation)) {
    auto *new_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::CastOp>(
        GET_MLIR_CONTEXT(), RemapValue(cast_op->GetOperand()), cast_op->GetFromType(), cast_op->GetToType());
    value_map_[cast_op->GetRes()] = new_op->GetRes();
    return new_op;
  }
  if (auto *get_ptr = mlir::DynamicOrNull<mlir::var::VarGetPtrOp>(operation)) {
    std::vector<mlir::Value *> indices;
    for (auto *index : get_ptr->GetIndices()) {
      indices.emplace_back(RemapValue(index));
    }
    auto *new_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarGetPtrOp>(GET_MLIR_CONTEXT(),
                                                                               RemapValue(get_ptr->GetPtr()), indices);
    new_op->GetRes()->SetType(get_ptr->GetRes()->GetType());
    value_map_[get_ptr->GetRes()] = new_op->GetRes();
    return new_op;
  }
  if (auto *get_value = mlir::DynamicOrNull<mlir::var::VarGetValOp>(operation)) {
    auto *new_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarGetValOp>(GET_MLIR_CONTEXT(),
                                                                               RemapValue(get_value->GetPtr()));
    new_op->GetRes()->SetType(get_value->GetRes()->GetType());
    value_map_[get_value->GetRes()] = new_op->GetRes();
    return new_op;
  }

#define CLONE_BINARY_OP(OP_TYPE)                                                               \
  if (auto *binary_op = mlir::DynamicOrNull<mlir::arithm::OP_TYPE##Op>(operation)) {           \
    auto *new_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::OP_TYPE##Op>(             \
        GET_MLIR_CONTEXT(), RemapValue(binary_op->GetLHS()), RemapValue(binary_op->GetRHS())); \
    new_op->GetRes()->SetType(binary_op->GetRes()->GetType());                                 \
    value_map_[binary_op->GetRes()] = new_op->GetRes();                                        \
    return new_op;                                                                             \
  }

  CLONE_BINARY_OP(Add)
  CLONE_BINARY_OP(Sub)
  CLONE_BINARY_OP(Mul)
  CLONE_BINARY_OP(Div)
  CLONE_BINARY_OP(Mod)
  CLONE_BINARY_OP(Equal)
  CLONE_BINARY_OP(NotEqual)
  CLONE_BINARY_OP(Less)
  CLONE_BINARY_OP(LessEqual)
  CLONE_BINARY_OP(Greater)
  CLONE_BINARY_OP(GreaterEqual)

#undef CLONE_BINARY_OP

#define CLONE_UNARY_OP(OP_TYPE)                                                                                        \
  if (auto *unary_op = mlir::DynamicOrNull<mlir::arithm::OP_TYPE##Op>(operation)) {                                    \
    auto *new_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::OP_TYPE##Op>(GET_MLIR_CONTEXT(),                  \
                                                                                  RemapValue(unary_op->GetOperand())); \
    new_op->GetRes()->SetType(unary_op->GetRes()->GetType());                                                          \
    value_map_[unary_op->GetRes()] = new_op->GetRes();                                                                 \
    return new_op;                                                                                                     \
  }

  CLONE_UNARY_OP(Neg)
  CLONE_UNARY_OP(Not)

#undef CLONE_UNARY_OP

  throw std::logic_error("Unsupported operation in MLIRTailRecursionEliminatePass clone: " + operation->Name());
}

auto MLIRTailRecursionEliminatePass::RebuildUsesInOperation(mlir::Operation *operation) -> void {
  for (size_t i = 0; i < operation->operands_.size(); ++i) {
    auto *operand = operation->operands_[i];
    if (operand != nullptr) {
      operand->AddUse(operation, i);
    }
  }
  for (auto *region : operation->regions_) {
    for (auto *block : region->blocks_) {
      for (auto *argument : block->arguments_) {
        if (argument != nullptr) {
          argument->ClearUses();
        }
      }
      for (auto *nested_op : block->operations_) {
        for (auto *result : nested_op->results_) {
          if (result != nullptr) {
            result->ClearUses();
          }
        }
      }
      for (auto *nested_op : block->operations_) {
        RebuildUsesInOperation(nested_op);
      }
    }
  }
}

auto MLIRTailRecursionEliminatePass::RebuildUses(mlir::func::FuncDefOp *func_op) -> void {
  for (auto *argument : func_op->GetArguments()) {
    if (argument != nullptr) {
      argument->ClearUses();
    }
  }
  for (auto *region : func_op->regions_) {
    for (auto *block : region->blocks_) {
      for (auto *operation : block->operations_) {
        for (auto *result : operation->results_) {
          if (result != nullptr) {
            result->ClearUses();
          }
        }
      }
      for (auto *operation : block->operations_) {
        RebuildUsesInOperation(operation);
      }
    }
  }
}

}  // namespace middleend::pass::transform
