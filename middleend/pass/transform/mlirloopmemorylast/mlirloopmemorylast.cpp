#include "middleend/pass/transform/mlirloopmemorylast/mlirloopmemorylast.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <unordered_set>
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

struct PointerAccess {
  mlir::Value *root{nullptr};
  std::vector<mlir::Value *> indices;
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

auto TraceConstInt(mlir::Value *value) -> std::pair<bool, int> {
  value = TraceCopies(value);
  auto *constant = value == nullptr ? nullptr : mlir::DynamicOrNull<mlir::arithm::ConstOp>(value->op_);
  if (constant == nullptr || !std::holds_alternative<int>(constant->GetValue())) {
    return {false, 0};
  }
  return {true, std::get<int>(constant->GetValue())};
}

auto ReadPointer(mlir::Value *pointer) -> PointerAccess {
  PointerAccess access;
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

auto CollectOperations(mlir::Region *region, std::vector<mlir::Operation *> *operations,
                       std::vector<mlir::scf::WhileOp *> *loops) -> void {
  if (region == nullptr) {
    return;
  }
  for (auto *block : region->blocks_) {
    for (auto *operation : block->operations_) {
      operations->emplace_back(operation);
      if (auto *loop = mlir::DynamicOrNull<mlir::scf::WhileOp>(operation)) {
        loops->emplace_back(loop);
      }
      for (auto *nested : operation->regions_) {
        CollectOperations(nested, operations, loops);
      }
    }
  }
}

auto IsNonNegative(mlir::Value *value, const std::unordered_set<mlir::Value *> &in_domain) -> bool {
  value = TraceCopies(value);
  if (value == nullptr) {
    return false;
  }
  if (in_domain.count(value) > 0) {
    return true;
  }
  auto [constant, number] = TraceConstInt(value);
  if (constant) {
    return number >= 0;
  }
  auto *add = mlir::DynamicOrNull<mlir::arithm::AddOp>(value->op_);
  if (add == nullptr) {
    return false;
  }
  auto [lhs_constant, lhs] = TraceConstInt(add->GetLHS());
  auto [rhs_constant, rhs] = TraceConstInt(add->GetRHS());
  return (IsNonNegative(add->GetLHS(), in_domain) && rhs_constant && rhs >= 0) ||
         (IsNonNegative(add->GetRHS(), in_domain) && lhs_constant && lhs >= 0);
}

auto MarkRectangleInductions(const std::vector<mlir::scf::WhileOp *> &loops, mlir::Value *bound,
                             std::unordered_set<mlir::Value *> *in_domain) -> void {
  bool changed = true;
  for (size_t iteration = 0; changed && iteration < loops.size() + 1; ++iteration) {
    changed = false;
    for (auto *loop : loops) {
      auto induction = loopiv::Recognize(loop);
      if (!induction.valid_ || induction.kind_ != loopiv::CmpKind::kLT ||
          induction.step_kind_ != loopiv::StepKind::kAdd || induction.step_ != 1 ||
          TraceCopies(induction.bound_) != TraceCopies(bound) || !IsNonNegative(induction.init_, *in_domain) ||
          loop->GetCondRegion()->blocks_.size() != 1 || loop->GetBodyRegion()->blocks_.size() != 1) {
        continue;
      }
      changed = in_domain->insert(loop->GetCondRegion()->blocks_[0]->arguments_[induction.index_]).second || changed;
      changed = in_domain->insert(loop->GetBodyRegion()->blocks_[0]->arguments_[induction.index_]).second || changed;
    }
  }
}

auto IsControlOnlyInduction(mlir::scf::WhileOp *loop, size_t index) -> bool {
  auto *cond_block = loop->GetCondRegion()->blocks_[0];
  auto *body = loop->GetBodyRegion()->blocks_[0];
  auto *condition_value = TraceCopies(cond_block->operations_.back()->operands_[0]);
  auto *compare =
      condition_value == nullptr ? nullptr : mlir::DynamicOrNull<mlir::arithm::LessOp>(condition_value->op_);
  auto *yield = mlir::DynamicOrNull<mlir::scf::YieldOp>(body->operations_.back());
  if (compare == nullptr || yield == nullptr || index >= yield->GetOperands().size()) {
    return false;
  }
  auto *increment_value = TraceCopies(yield->GetOperands()[index]);
  auto *increment =
      increment_value == nullptr ? nullptr : mlir::DynamicOrNull<mlir::arithm::AddOp>(increment_value->op_);
  if (increment == nullptr) {
    return false;
  }

  std::unordered_set<mlir::Value *> family = {cond_block->arguments_[index], body->arguments_[index]};
  bool changed = true;
  while (changed) {
    changed = false;
    std::vector<mlir::Value *> values(family.begin(), family.end());
    for (auto *value : values) {
      for (auto *use : value->GetUses()) {
        auto *operation = use->operation_;
        if (auto *copy = mlir::DynamicOrNull<mlir::arithm::CopyOp>(operation)) {
          changed = family.insert(copy->GetRes()).second || changed;
        } else if (operation == increment) {
          changed = family.insert(increment->GetRes()).second || changed;
        }
      }
    }
  }
  for (auto *value : family) {
    for (auto *use : value->GetUses()) {
      auto *operation = use->operation_;
      if (operation == compare || operation == increment ||
          mlir::DynamicOrNull<mlir::arithm::CopyOp>(operation) != nullptr ||
          mlir::DynamicOrNull<mlir::scf::ConditionOp>(operation) != nullptr ||
          mlir::DynamicOrNull<mlir::scf::YieldOp>(operation) != nullptr) {
        continue;
      }
      return false;
    }
  }
  return true;
}

auto FindFirstNestedLoop(mlir::Block *body, size_t *index) -> mlir::scf::WhileOp * {
  for (size_t current = 0; current + 1 < body->operations_.size(); ++current) {
    if (auto *loop = mlir::DynamicOrNull<mlir::scf::WhileOp>(body->operations_[current])) {
      *index = current;
      return loop;
    }
    if (mlir::DynamicOrNull<mlir::arithm::CopyOp>(body->operations_[current]) == nullptr &&
        mlir::DynamicOrNull<mlir::arithm::ConstOp>(body->operations_[current]) == nullptr) {
      return nullptr;
    }
  }
  return nullptr;
}

auto FindResetStore(mlir::scf::WhileOp *reset_outer, mlir::Value *bound, mlir::Value **root) -> bool {
  auto outer_induction = loopiv::Recognize(reset_outer);
  auto [outer_zero, outer_init] = TraceConstInt(outer_induction.init_);
  if (!outer_induction.valid_ || outer_induction.kind_ != loopiv::CmpKind::kLT ||
      outer_induction.step_kind_ != loopiv::StepKind::kAdd || outer_induction.step_ != 1 || !outer_zero ||
      outer_init != 0 || TraceCopies(outer_induction.bound_) != TraceCopies(bound) ||
      reset_outer->GetBodyRegion()->blocks_.size() != 1) {
    return false;
  }
  auto *outer_iv = reset_outer->GetBodyRegion()->blocks_[0]->arguments_[outer_induction.index_];
  std::vector<mlir::Operation *> operations;
  std::vector<mlir::scf::WhileOp *> loops;
  CollectOperations(reset_outer->GetBodyRegion(), &operations, &loops);

  mlir::scf::WhileOp *inner = nullptr;
  for (auto *candidate : loops) {
    auto induction = loopiv::Recognize(candidate);
    auto [zero, init] = TraceConstInt(induction.init_);
    if (induction.valid_ && induction.kind_ == loopiv::CmpKind::kLT && induction.step_kind_ == loopiv::StepKind::kAdd &&
        induction.step_ == 1 && zero && init == 0 && TraceCopies(induction.bound_) == TraceCopies(bound)) {
      inner = candidate;
      break;
    }
  }
  if (inner == nullptr) {
    return false;
  }
  auto inner_induction = loopiv::Recognize(inner);
  auto *inner_iv = inner->GetBodyRegion()->blocks_[0]->arguments_[inner_induction.index_];

  mlir::var::VarSetOp *reset_store = nullptr;
  for (auto *operation : operations) {
    if (mlir::DynamicOrNull<mlir::func::FuncCallOp>(operation) != nullptr ||
        mlir::DynamicOrNull<mlir::scf::ContinueOp>(operation) != nullptr ||
        mlir::DynamicOrNull<mlir::scf::BreakOp>(operation) != nullptr) {
      return false;
    }
    if (auto *load = mlir::DynamicOrNull<mlir::var::VarGetValOp>(operation)) {
      if (*root != nullptr && ReadPointer(load->GetPtr()).root == *root) {
        return false;
      }
    }
    auto *store = mlir::DynamicOrNull<mlir::var::VarSetOp>(operation);
    if (store == nullptr) {
      continue;
    }
    auto access = ReadPointer(store->GetPtr());
    if (access.root == nullptr || access.indices.size() != 2 ||
        TraceCopies(access.indices[0]) != TraceCopies(outer_iv) ||
        TraceCopies(access.indices[1]) != TraceCopies(inner_iv) || reset_store != nullptr) {
      return false;
    }
    reset_store = store;
    *root = access.root;
  }
  if (reset_store == nullptr) {
    return false;
  }
  // Recheck loads now that the reset root is known.
  for (auto *operation : operations) {
    auto *load = mlir::DynamicOrNull<mlir::var::VarGetValOp>(operation);
    if (load != nullptr && ReadPointer(load->GetPtr()).root == *root) {
      return false;
    }
  }
  return true;
}

}  // namespace

auto MLIRLoopMemoryLastIterationPass::Run() -> void {
  PreRun();
  if (std::getenv("LOOP_MEMORY_LAST_OFF") != nullptr) {
    return;
  }
  loops_seen_ = 0;
  candidates_ = 0;
  folded_ = 0;
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
    std::cerr << "LoopMemoryLast: candidates=" << candidates_ << " folded=" << folded_ << " loops=" << loops_seen_
              << '\n';
  }
}

auto MLIRLoopMemoryLastIterationPass::RunOnFunction(mlir::func::FuncDefOp *function) -> bool {
  mlir::utils::RebuildFunctionUses(function);
  bool changed = FoldInBlock(function, function->GetEntryBlock());
  if (changed) {
    mlir::utils::RebuildFunctionUses(function);
  }
  return changed;
}

auto MLIRLoopMemoryLastIterationPass::FoldInBlock(mlir::func::FuncDefOp *function, mlir::Block *block) -> bool {
  bool changed = false;
  std::vector<mlir::Operation *> original = block->operations_;
  for (auto *operation : original) {
    for (auto *region : operation->regions_) {
      for (auto *nested : region->blocks_) {
        changed = FoldInBlock(function, nested) || changed;
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
    Fold(function, block, index, loop, candidate);
    ++folded_;
    changed = true;
    ++index;
  }
  return changed;
}

auto MLIRLoopMemoryLastIterationPass::FindCandidate(mlir::scf::WhileOp *loop, Candidate *candidate) -> bool {
  ++loops_seen_;
  auto induction = loopiv::Recognize(loop);
  int64_t trip_count = loopiv::ConstantTripCount(induction, 1024);
  auto [zero, init] = TraceConstInt(induction.init_);
  if (!induction.valid_ || induction.kind_ != loopiv::CmpKind::kLT || induction.step_kind_ != loopiv::StepKind::kAdd ||
      induction.step_ != 1 || !zero || init != 0 || trip_count <= 1 || loop->GetBodyRegion()->blocks_.size() != 1 ||
      loop->GetCondRegion()->blocks_.size() != 1 || !IsControlOnlyInduction(loop, induction.index_)) {
    return false;
  }
  for (auto *result : loop->results_) {
    if (!result->GetUses().empty()) {
      return false;
    }
  }

  auto *body = loop->GetBodyRegion()->blocks_[0];
  size_t reset_index = 0;
  auto *reset_loop = FindFirstNestedLoop(body, &reset_index);
  mlir::Value *reset_root = nullptr;
  if (reset_loop == nullptr) {
    return false;
  }
  auto reset_induction = loopiv::Recognize(reset_loop);
  if (!reset_induction.valid_ || !FindResetStore(reset_loop, reset_induction.bound_, &reset_root)) {
    return false;
  }
  mlir::Value *rectangle_bound = reset_induction.bound_;

  std::vector<mlir::Operation *> operations;
  std::vector<mlir::scf::WhileOp *> loops;
  CollectOperations(loop->GetBodyRegion(), &operations, &loops);
  std::unordered_set<mlir::Value *> in_domain;
  MarkRectangleInductions(loops, rectangle_bound, &in_domain);

  for (auto *operation : operations) {
    if (mlir::DynamicOrNull<mlir::func::FuncCallOp>(operation) != nullptr ||
        mlir::DynamicOrNull<mlir::scf::ContinueOp>(operation) != nullptr ||
        mlir::DynamicOrNull<mlir::scf::BreakOp>(operation) != nullptr) {
      return false;
    }
    mlir::Value *pointer = nullptr;
    bool is_store = false;
    if (auto *load = mlir::DynamicOrNull<mlir::var::VarGetValOp>(operation)) {
      pointer = load->GetPtr();
    } else if (auto *store = mlir::DynamicOrNull<mlir::var::VarSetOp>(operation)) {
      pointer = store->GetPtr();
      is_store = true;
    } else {
      continue;
    }
    auto access = ReadPointer(pointer);
    if (is_store && access.root != reset_root) {
      return false;
    }
    if (access.root != reset_root) {
      continue;
    }
    if (access.indices.size() != 2 ||
        std::any_of(access.indices.begin(), access.indices.end(),
                    [&](mlir::Value *index) { return in_domain.count(TraceCopies(index)) == 0; })) {
      return false;
    }
  }

  auto *condition_value = TraceCopies(loop->GetCondRegion()->blocks_[0]->operations_.back()->operands_[0]);
  auto *condition_less =
      condition_value == nullptr ? nullptr : mlir::DynamicOrNull<mlir::arithm::LessOp>(condition_value->op_);
  if (condition_less == nullptr) {
    return false;
  }
  candidate->condition_compare = condition_less;
  return true;
}

auto MLIRLoopMemoryLastIterationPass::Fold(mlir::func::FuncDefOp *function, mlir::Block *parent, size_t loop_index,
                                           [[maybe_unused]] mlir::scf::WhileOp *loop,
                                           const Candidate &candidate) -> void {
  auto *compare = mlir::DynamicOrNull<mlir::arithm::LessOp>(candidate.condition_compare);
  auto *one = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::ConstOp>(GET_MLIR_CONTEXT(), 1);
  one->GetRes()->SetType(compare->GetRHS()->GetType());
  compare->operands_[1] = one->GetRes();
  parent->operations_.insert(parent->operations_.begin() + static_cast<std::ptrdiff_t>(loop_index), one);
  mlir::utils::RebuildFunctionUses(function);
}

}  // namespace middleend::pass::transform
