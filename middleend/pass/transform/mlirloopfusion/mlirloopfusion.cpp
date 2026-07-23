#include "middleend/pass/transform/mlirloopfusion/mlirloopfusion.h"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <limits>
#include <string>
#include <unordered_set>
#include <utility>
#include <variant>
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
#include "middleend/mlir/dialect/var/def.h"
#include "middleend/mlir/dialect/var/getptr.h"
#include "middleend/mlir/dialect/var/getvalue.h"
#include "middleend/mlir/dialect/var/globaldef.h"
#include "middleend/mlir/dialect/var/set.h"
#include "middleend/mlir/utils/clone/uselistrebuilder.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/block.h"
#include "middleend/mlir/utils/struct/region.h"
#include "middleend/mlir/utils/value/value.h"
#include "middleend/pass/transform/mlirloopunroll/inductionvariable.h"

#define GET_MLIR_CONTEXT() compiler_context_->GetIRContext()->GetMLIRContext()

namespace middleend::pass::transform {
namespace {

enum class RootKind : uint8_t { kLocal, kGlobal, kParameter, kUnknown };

struct PointerRoot {
  RootKind kind{RootKind::kUnknown};
  mlir::Value *value{nullptr};
};

struct DirectCall {
  std::string name;
  std::vector<mlir::Value *> arguments;
};

struct AffineIndex {
  int64_t coefficient{0};
  int64_t offset{0};
};

auto TraceCopies(mlir::Value *value) -> mlir::Value * {
  std::unordered_set<mlir::Value *> visited;
  while (value != nullptr && visited.insert(value).second) {
    auto *copy = mlir::DynamicOrNull<mlir::arithm::CopyOp>(value->op_);
    if (copy == nullptr) {
      break;
    }
    value = copy->GetValue();
  }
  return value;
}

auto SameConstant(mlir::Value *lhs, mlir::Value *rhs) -> bool {
  auto *lhs_const = lhs == nullptr ? nullptr : mlir::DynamicOrNull<mlir::arithm::ConstOp>(lhs->op_);
  auto *rhs_const = rhs == nullptr ? nullptr : mlir::DynamicOrNull<mlir::arithm::ConstOp>(rhs->op_);
  return lhs_const != nullptr && rhs_const != nullptr && lhs_const->GetValue() == rhs_const->GetValue();
}

auto SameValue(mlir::Value *lhs, mlir::Value *rhs) -> bool {
  lhs = TraceCopies(lhs);
  rhs = TraceCopies(rhs);
  return lhs == rhs || SameConstant(lhs, rhs);
}

auto GetPointerRoot(mlir::Value *pointer) -> PointerRoot {
  pointer = TraceCopies(pointer);
  std::unordered_set<mlir::Value *> visited;
  while (pointer != nullptr && visited.insert(pointer).second) {
    if (auto *get_ptr = mlir::DynamicOrNull<mlir::var::VarGetPtrOp>(pointer->op_)) {
      pointer = TraceCopies(get_ptr->GetPtr());
      continue;
    }
    if (mlir::DynamicOrNull<mlir::var::VarDefOp>(pointer->op_) != nullptr) {
      return {RootKind::kLocal, pointer};
    }
    if (mlir::DynamicOrNull<mlir::var::GlobalVarDefOp>(pointer->op_) != nullptr) {
      return {RootKind::kGlobal, pointer};
    }
    if (pointer->op_ == nullptr) {
      return {RootKind::kParameter, pointer};
    }
    return {RootKind::kUnknown, pointer};
  }
  return {};
}

auto RootsAreDistinct(const PointerRoot &lhs, const PointerRoot &rhs) -> bool {
  if (lhs.value == nullptr || rhs.value == nullptr || lhs.value == rhs.value) {
    return false;
  }
  if (lhs.kind == RootKind::kLocal || rhs.kind == RootKind::kLocal) {
    return true;
  }
  return lhs.kind == RootKind::kGlobal && rhs.kind == RootKind::kGlobal;
}

auto GetConstantInt(mlir::Value *value, int64_t *constant) -> bool {
  value = TraceCopies(value);
  auto *constant_op = value == nullptr ? nullptr : mlir::DynamicOrNull<mlir::arithm::ConstOp>(value->op_);
  if (constant_op == nullptr || !std::holds_alternative<int>(constant_op->GetValue())) {
    return false;
  }
  *constant = std::get<int>(constant_op->GetValue());
  return true;
}

auto CheckedAffine(int64_t coefficient, int64_t offset, AffineIndex *result) -> bool {
  result->coefficient = coefficient;
  result->offset = offset;
  return true;
}

auto ExtractAffineIndex(mlir::Value *value, mlir::Value *induction, AffineIndex *result, unsigned depth = 0) -> bool {
  if (depth > 32) {
    return false;
  }
  value = TraceCopies(value);
  induction = TraceCopies(induction);
  if (value == induction) {
    return CheckedAffine(1, 0, result);
  }
  int64_t constant = 0;
  if (GetConstantInt(value, &constant)) {
    return CheckedAffine(0, constant, result);
  }
  if (value == nullptr || value->op_ == nullptr) {
    return false;
  }

  auto combine = [&](mlir::Value *lhs_value, mlir::Value *rhs_value, bool subtract) -> bool {
    AffineIndex lhs;
    AffineIndex rhs;
    if (!ExtractAffineIndex(lhs_value, induction, &lhs, depth + 1) ||
        !ExtractAffineIndex(rhs_value, induction, &rhs, depth + 1)) {
      return false;
    }
    __int128 coefficient = static_cast<__int128>(lhs.coefficient) + (subtract ? -static_cast<__int128>(rhs.coefficient)
                                                                              : static_cast<__int128>(rhs.coefficient));
    __int128 offset = static_cast<__int128>(lhs.offset) +
                      (subtract ? -static_cast<__int128>(rhs.offset) : static_cast<__int128>(rhs.offset));
    if (coefficient < INT64_MIN || coefficient > INT64_MAX || offset < INT64_MIN || offset > INT64_MAX) {
      return false;
    }
    return CheckedAffine(static_cast<int64_t>(coefficient), static_cast<int64_t>(offset), result);
  };

  if (auto *add = mlir::DynamicOrNull<mlir::arithm::AddOp>(value->op_)) {
    return combine(add->GetLHS(), add->GetRHS(), false);
  }
  if (auto *sub = mlir::DynamicOrNull<mlir::arithm::SubOp>(value->op_)) {
    return combine(sub->GetLHS(), sub->GetRHS(), true);
  }
  if (auto *neg = mlir::DynamicOrNull<mlir::arithm::NegOp>(value->op_)) {
    AffineIndex operand;
    if (!ExtractAffineIndex(neg->GetOperand(), induction, &operand, depth + 1) || operand.coefficient == INT64_MIN ||
        operand.offset == INT64_MIN) {
      return false;
    }
    return CheckedAffine(-operand.coefficient, -operand.offset, result);
  }
  if (auto *multiply = mlir::DynamicOrNull<mlir::arithm::MulOp>(value->op_)) {
    mlir::Value *affine_value = multiply->GetLHS();
    mlir::Value *constant_value = multiply->GetRHS();
    if (!GetConstantInt(constant_value, &constant)) {
      std::swap(affine_value, constant_value);
      if (!GetConstantInt(constant_value, &constant)) {
        return false;
      }
    }
    AffineIndex affine;
    if (!ExtractAffineIndex(affine_value, induction, &affine, depth + 1)) {
      return false;
    }
    __int128 coefficient = static_cast<__int128>(affine.coefficient) * constant;
    __int128 offset = static_cast<__int128>(affine.offset) * constant;
    if (coefficient < INT64_MIN || coefficient > INT64_MAX || offset < INT64_MIN || offset > INT64_MAX) {
      return false;
    }
    return CheckedAffine(static_cast<int64_t>(coefficient), static_cast<int64_t>(offset), result);
  }
  return false;
}

auto CollectPointerIndices(mlir::Value *pointer, std::vector<mlir::Value *> *indices) -> void {
  pointer = TraceCopies(pointer);
  auto *get_ptr = pointer == nullptr ? nullptr : mlir::DynamicOrNull<mlir::var::VarGetPtrOp>(pointer->op_);
  if (get_ptr == nullptr) {
    return;
  }
  CollectPointerIndices(get_ptr->GetPtr(), indices);
  auto current = get_ptr->GetIndices();
  indices->insert(indices->end(), current.begin(), current.end());
}

auto StructurallyEqual(mlir::Value *lhs, mlir::Value *rhs,
                       const std::unordered_map<mlir::Value *, mlir::Value *> &rhs_to_lhs, unsigned depth = 0) -> bool {
  if (depth > 32) {
    return false;
  }
  lhs = TraceCopies(lhs);
  rhs = TraceCopies(rhs);
  auto mapped = rhs_to_lhs.find(rhs);
  if (mapped != rhs_to_lhs.end()) {
    rhs = TraceCopies(mapped->second);
  }
  if (lhs == rhs || SameConstant(lhs, rhs)) {
    return true;
  }
  if ((lhs != nullptr && (mlir::DynamicOrNull<mlir::var::VarDefOp>(lhs->op_) != nullptr ||
                          mlir::DynamicOrNull<mlir::var::GlobalVarDefOp>(lhs->op_) != nullptr)) ||
      (rhs != nullptr && (mlir::DynamicOrNull<mlir::var::VarDefOp>(rhs->op_) != nullptr ||
                          mlir::DynamicOrNull<mlir::var::GlobalVarDefOp>(rhs->op_) != nullptr))) {
    return false;
  }
  if (lhs == nullptr || rhs == nullptr || lhs->op_ == nullptr || rhs->op_ == nullptr ||
      lhs->op_->Name() != rhs->op_->Name() || lhs->op_->operands_.size() != rhs->op_->operands_.size()) {
    return false;
  }
  for (size_t index = 0; index < lhs->op_->operands_.size(); ++index) {
    if (!StructurallyEqual(lhs->op_->operands_[index], rhs->op_->operands_[index], rhs_to_lhs, depth + 1)) {
      return false;
    }
  }
  return true;
}

auto HasDirectInductionIndex(mlir::Value *first_pointer, mlir::Value *second_pointer, mlir::Value *first_iv,
                             mlir::Value *second_iv,
                             const std::unordered_map<mlir::Value *, mlir::Value *> &argument_map) -> bool {
  first_pointer = TraceCopies(first_pointer);
  second_pointer = TraceCopies(second_pointer);
  while (first_pointer != nullptr && second_pointer != nullptr) {
    auto *first_get_ptr = mlir::DynamicOrNull<mlir::var::VarGetPtrOp>(first_pointer->op_);
    auto *second_get_ptr = mlir::DynamicOrNull<mlir::var::VarGetPtrOp>(second_pointer->op_);
    if (first_get_ptr == nullptr || second_get_ptr == nullptr) {
      return false;
    }
    auto first_indices = first_get_ptr->GetIndices();
    auto second_indices = second_get_ptr->GetIndices();
    if (first_indices.size() != second_indices.size()) {
      return false;
    }
    for (size_t index = 0; index < first_indices.size(); ++index) {
      if (SameValue(first_indices[index], first_iv) && SameValue(second_indices[index], second_iv)) {
        return true;
      }
    }
    first_pointer = TraceCopies(first_get_ptr->GetPtr());
    second_pointer = TraceCopies(second_get_ptr->GetPtr());
    if (!StructurallyEqual(first_pointer, second_pointer, argument_map)) {
      return false;
    }
  }
  return false;
}

auto AffineDependencesAllowFusion(mlir::Value *first_pointer, mlir::Value *second_pointer, mlir::Value *first_iv,
                                  mlir::Value *second_iv,
                                  const std::unordered_map<mlir::Value *, mlir::Value *> &argument_map) -> bool {
  std::vector<mlir::Value *> first_indices;
  std::vector<mlir::Value *> second_indices;
  CollectPointerIndices(first_pointer, &first_indices);
  CollectPointerIndices(second_pointer, &second_indices);
  if (first_indices.empty() || first_indices.size() != second_indices.size()) {
    return false;
  }

  bool have_distance = false;
  int64_t common_distance = 0;
  for (size_t index = 0; index < first_indices.size(); ++index) {
    AffineIndex lhs;
    AffineIndex rhs;
    bool lhs_affine = ExtractAffineIndex(first_indices[index], first_iv, &lhs);
    bool rhs_affine = ExtractAffineIndex(second_indices[index], second_iv, &rhs);
    if (!lhs_affine || !rhs_affine) {
      if (!StructurallyEqual(first_indices[index], second_indices[index], argument_map)) {
        return false;
      }
      continue;
    }
    if (lhs.coefficient != rhs.coefficient) {
      return false;
    }
    if (lhs.coefficient == 0) {
      if (lhs.offset != rhs.offset) {
        return true;  // This dimension is always different: the accesses never alias.
      }
      continue;
    }
    __int128 delta = static_cast<__int128>(rhs.offset) - static_cast<__int128>(lhs.offset);
    if (delta % lhs.coefficient != 0) {
      return true;  // No integer iteration pair can make this dimension equal.
    }
    __int128 wide_distance = delta / lhs.coefficient;
    if (wide_distance < std::numeric_limits<int64_t>::min() || wide_distance > std::numeric_limits<int64_t>::max()) {
      return false;
    }
    int64_t distance = static_cast<int64_t>(wide_distance);
    if (have_distance && distance != common_distance) {
      return true;  // Different dimensions require incompatible iteration distances.
    }
    common_distance = distance;
    have_distance = true;
  }

  // A positive distance means second(j) aliases first(i) with i > j. Fusion
  // would move second(j) before that future first(i), reversing the original
  // all-first-before-all-second order. Zero and negative distances preserve it.
  return have_distance && common_distance <= 0;
}

auto UsesAnyResult(mlir::Operation *operation, const std::unordered_set<mlir::Value *> &results) -> bool {
  for (auto *operand : operation->operands_) {
    if (results.count(TraceCopies(operand)) > 0) {
      return true;
    }
  }
  if (auto *condition = mlir::DynamicOrNull<mlir::scf::ConditionOp>(operation)) {
    for (auto *original : condition->GetOriginalValues()) {
      if (results.count(TraceCopies(original)) > 0) {
        return true;
      }
    }
  }
  for (auto *region : operation->regions_) {
    for (auto *block : region->blocks_) {
      for (auto *nested : block->operations_) {
        if (UsesAnyResult(nested, results)) {
          return true;
        }
      }
    }
  }
  return false;
}

auto IsHoistableGapOperation(mlir::Operation *operation) -> bool {
  return mlir::DynamicOrNull<mlir::arithm::ConstOp>(operation) != nullptr ||
         mlir::DynamicOrNull<mlir::arithm::CopyOp>(operation) != nullptr ||
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
         mlir::DynamicOrNull<mlir::arithm::NegOp>(operation) != nullptr ||
         mlir::DynamicOrNull<mlir::arithm::NotOp>(operation) != nullptr;
}

auto GetLoopParts(mlir::scf::WhileOp *loop, mlir::Block **condition_block, mlir::Block **body_block,
                  mlir::scf::ConditionOp **condition, mlir::scf::YieldOp **yield) -> bool {
  if (loop == nullptr || loop->GetCondRegion()->blocks_.size() != 1 || loop->GetBodyRegion()->blocks_.size() != 1) {
    return false;
  }
  *condition_block = loop->GetCondRegion()->blocks_[0];
  *body_block = loop->GetBodyRegion()->blocks_[0];
  if ((*condition_block)->operations_.empty() || (*body_block)->operations_.empty()) {
    return false;
  }
  *condition = mlir::DynamicOrNull<mlir::scf::ConditionOp>((*condition_block)->operations_.back());
  *yield = mlir::DynamicOrNull<mlir::scf::YieldOp>((*body_block)->operations_.back());
  return *condition != nullptr && *yield != nullptr;
}

}  // namespace

auto MLIRLoopFusionPass::Run() -> void {
  fused_loops_ = 0;
  candidates_ = 0;
  domain_rejects_ = 0;
  shape_rejects_ = 0;
  scalar_rejects_ = 0;
  memory_rejects_ = 0;
  noalias_parameters_.clear();
  auto *module = GET_MLIR_CONTEXT()->GetModuleOp();
  if (module == nullptr) {
    throw std::logic_error("Module operation is not set in MLIRContext");
  }
  CollectInterproceduralNoAliasFacts();
  for (auto *operation : module->GetBlock()->operations_) {
    auto *function = mlir::DynamicOrNull<mlir::func::FuncDefOp>(operation);
    if (function != nullptr) {
      RunOnFunction(function);
    }
  }
  if (std::getenv("LOOP_OPT_STATS") != nullptr) {
    std::cerr << "LoopFusion: candidates=" << candidates_ << " fused=" << fused_loops_ << " domain=" << domain_rejects_
              << " shape=" << shape_rejects_ << " scalar=" << scalar_rejects_ << " memory=" << memory_rejects_ << '\n';
  }
}

auto MLIRLoopFusionPass::CollectInterproceduralNoAliasFacts() -> void {
  auto *module = GET_MLIR_CONTEXT()->GetModuleOp();
  std::unordered_map<std::string, mlir::func::FuncDefOp *> functions;
  std::unordered_map<std::string, std::vector<DirectCall>> calls_by_callee;

  std::function<void(mlir::Operation *)> collect_calls = [&](mlir::Operation *operation) {
    if (auto *call = mlir::DynamicOrNull<mlir::func::FuncCallOp>(operation)) {
      calls_by_callee[call->GetName()].push_back({call->GetName(), call->GetOperands()});
    }
    for (auto *region : operation->regions_) {
      for (auto *block : region->blocks_) {
        for (auto *nested : block->operations_) {
          collect_calls(nested);
        }
      }
    }
  };

  for (auto *operation : module->GetBlock()->operations_) {
    auto *function = mlir::DynamicOrNull<mlir::func::FuncDefOp>(operation);
    if (function == nullptr) {
      continue;
    }
    functions[function->GetName()] = function;
    collect_calls(function);
  }

  bool changed = true;
  while (changed) {
    changed = false;
    for (const auto &[name, function] : functions) {
      auto calls = calls_by_callee.find(name);
      if (calls == calls_by_callee.end() || calls->second.empty()) {
        continue;
      }
      auto parameters = function->GetArguments();
      for (size_t lhs_index = 0; lhs_index < parameters.size(); ++lhs_index) {
        auto *lhs = parameters[lhs_index];
        if (lhs == nullptr || lhs->GetType() == nullptr || lhs->GetType()->GetKind() != type::TypeKind::PTR) {
          continue;
        }
        for (size_t rhs_index = lhs_index + 1; rhs_index < parameters.size(); ++rhs_index) {
          auto *rhs = parameters[rhs_index];
          if (rhs == nullptr || rhs->GetType() == nullptr || rhs->GetType()->GetKind() != type::TypeKind::PTR ||
              AreProvenNoAliasParameters(lhs, rhs)) {
            continue;
          }
          bool every_call_is_distinct =
              std::all_of(calls->second.begin(), calls->second.end(), [&](const DirectCall &call) {
                if (lhs_index >= call.arguments.size() || rhs_index >= call.arguments.size()) {
                  return false;
                }
                auto lhs_root = GetPointerRoot(call.arguments[lhs_index]);
                auto rhs_root = GetPointerRoot(call.arguments[rhs_index]);
                if (RootsAreDistinct(lhs_root, rhs_root)) {
                  return true;
                }
                return lhs_root.kind == RootKind::kParameter && rhs_root.kind == RootKind::kParameter &&
                       AreProvenNoAliasParameters(lhs_root.value, rhs_root.value);
              });
          if (every_call_is_distinct) {
            noalias_parameters_[lhs].insert(rhs);
            noalias_parameters_[rhs].insert(lhs);
            changed = true;
          }
        }
      }
    }
  }
}

auto MLIRLoopFusionPass::AreProvenNoAliasParameters(mlir::Value *lhs, mlir::Value *rhs) const -> bool {
  auto iter = noalias_parameters_.find(lhs);
  return iter != noalias_parameters_.end() && iter->second.count(rhs) > 0;
}

auto MLIRLoopFusionPass::RunOnFunction(mlir::func::FuncDefOp *func_op) -> bool {
  bool changed = FuseInBlock(func_op->GetEntryBlock());
  if (changed) {
    mlir::utils::RebuildFunctionUses(func_op);
  }
  return changed;
}

auto MLIRLoopFusionPass::FuseInBlock(mlir::Block *block) -> bool {
  bool changed = false;
  for (auto *operation : block->operations_) {
    for (auto *region : operation->regions_) {
      for (auto *nested : region->blocks_) {
        changed = FuseInBlock(nested) || changed;
      }
    }
  }

  for (size_t index = 0; index + 1 < block->operations_.size();) {
    auto *first = mlir::DynamicOrNull<mlir::scf::WhileOp>(block->operations_[index]);
    if (first == nullptr) {
      ++index;
      continue;
    }
    std::unordered_set<mlir::Value *> first_results(first->results_.begin(), first->results_.end());
    size_t second_index = index + 1;
    while (second_index < block->operations_.size() &&
           mlir::DynamicOrNull<mlir::scf::WhileOp>(block->operations_[second_index]) == nullptr &&
           IsHoistableGapOperation(block->operations_[second_index]) &&
           !UsesAnyResult(block->operations_[second_index], first_results)) {
      ++second_index;
    }
    auto *second = second_index < block->operations_.size()
                       ? mlir::DynamicOrNull<mlir::scf::WhileOp>(block->operations_[second_index])
                       : nullptr;
    if (second == nullptr) {
      ++index;
      continue;
    }
    ++candidates_;
    if (!CanFuse(first, second)) {
      ++index;
      continue;
    }
    std::rotate(block->operations_.begin() + static_cast<std::ptrdiff_t>(index),
                block->operations_.begin() + static_cast<std::ptrdiff_t>(index + 1),
                block->operations_.begin() + static_cast<std::ptrdiff_t>(second_index));
    Fuse(block, second_index - 1, first, second);
    ++fused_loops_;
    changed = true;
    index = 0;
  }
  return changed;
}

auto MLIRLoopFusionPass::CanFuse(mlir::scf::WhileOp *first, mlir::scf::WhileOp *second) -> bool {
  auto first_iv = loopiv::Recognize(first);
  auto second_iv = loopiv::Recognize(second);
  const std::unordered_map<mlir::Value *, mlir::Value *> no_substitutions;
  if (!first_iv.valid_ || !second_iv.valid_ || first_iv.kind_ != second_iv.kind_ ||
      first_iv.step_kind_ != second_iv.step_kind_ || first_iv.step_ != second_iv.step_ ||
      !StructurallyEqual(first_iv.init_, second_iv.init_, no_substitutions) ||
      !StructurallyEqual(first_iv.bound_, second_iv.bound_, no_substitutions) ||
      loopiv::IsDefinedInLoop(first_iv.bound_, first) || loopiv::IsDefinedInLoop(second_iv.bound_, second)) {
    ++domain_rejects_;
    return false;
  }

  mlir::Block *first_condition_block = nullptr;
  mlir::Block *first_body = nullptr;
  mlir::Block *second_condition_block = nullptr;
  mlir::Block *second_body = nullptr;
  mlir::scf::ConditionOp *first_condition = nullptr;
  mlir::scf::ConditionOp *second_condition = nullptr;
  mlir::scf::YieldOp *first_yield = nullptr;
  mlir::scf::YieldOp *second_yield = nullptr;
  if (!GetLoopParts(first, &first_condition_block, &first_body, &first_condition, &first_yield) ||
      !GetLoopParts(second, &second_condition_block, &second_body, &second_condition, &second_yield) ||
      first_condition_block->arguments_.size() != first_condition->GetOperands().size() ||
      second_condition_block->arguments_.size() != second_condition->GetOperands().size() ||
      first_body->arguments_.size() != first_condition_block->arguments_.size() ||
      second_body->arguments_.size() != second_condition_block->arguments_.size() ||
      first_yield->GetOperands().size() != first_condition_block->arguments_.size() ||
      second_yield->GetOperands().size() != second_condition_block->arguments_.size()) {
    ++shape_rejects_;
    return false;
  }
  for (size_t index = 0; index < second_condition_block->arguments_.size(); ++index) {
    if (!SameValue(second_condition->GetOperands()[index], second_condition_block->arguments_[index])) {
      ++shape_rejects_;
      return false;
    }
  }

  std::unordered_set<mlir::Value *> first_results(first->results_.begin(), first->results_.end());
  if (UsesAnyResult(second, first_results)) {
    ++scalar_rejects_;
    return false;
  }

  std::unordered_map<mlir::Value *, mlir::Value *> argument_map;
  if (first_iv.index_ >= first_condition_block->arguments_.size() ||
      second_iv.index_ >= second_condition_block->arguments_.size()) {
    ++shape_rejects_;
    return false;
  }
  argument_map[second_condition_block->arguments_[second_iv.index_]] =
      first_condition_block->arguments_[first_iv.index_];
  if (!DependencesAllowFusion(first, second, argument_map)) {
    ++memory_rejects_;
    return false;
  }
  return true;
}

auto MLIRLoopFusionPass::CollectMemoryAccesses(mlir::Operation *operation,
                                               std::vector<MemoryAccess> *accesses) -> bool {
  if (auto *load = mlir::DynamicOrNull<mlir::var::VarGetValOp>(operation)) {
    accesses->push_back({load->GetPtr(), false});
  } else if (auto *store = mlir::DynamicOrNull<mlir::var::VarSetOp>(operation)) {
    accesses->push_back({store->GetPtr(), true});
  } else if (mlir::DynamicOrNull<mlir::func::FuncCallOp>(operation) != nullptr ||
             mlir::DynamicOrNull<mlir::scf::WhileOp>(operation) != nullptr) {
    return false;
  }
  for (auto *region : operation->regions_) {
    for (auto *block : region->blocks_) {
      for (auto *nested : block->operations_) {
        if (!CollectMemoryAccesses(nested, accesses)) {
          return false;
        }
      }
    }
  }
  return true;
}

auto MLIRLoopFusionPass::DependencesAllowFusion(mlir::scf::WhileOp *first, mlir::scf::WhileOp *second,
                                                const std::unordered_map<mlir::Value *, mlir::Value *> &argument_map)
    -> bool {
  auto first_iv = loopiv::Recognize(first);
  auto second_iv = loopiv::Recognize(second);
  auto *first_body = first->GetBodyRegion()->blocks_[0];
  auto *second_body = second->GetBodyRegion()->blocks_[0];
  std::vector<MemoryAccess> first_accesses;
  std::vector<MemoryAccess> second_accesses;
  for (size_t index = 0; index + 1 < first_body->operations_.size(); ++index) {
    if (!CollectMemoryAccesses(first_body->operations_[index], &first_accesses)) {
      return false;
    }
  }
  for (size_t index = 0; index + 1 < second_body->operations_.size(); ++index) {
    if (!CollectMemoryAccesses(second_body->operations_[index], &second_accesses)) {
      return false;
    }
  }

  auto *first_iv_arg = first_body->arguments_[first_iv.index_];
  auto *second_iv_arg = second_body->arguments_[second_iv.index_];
  for (const auto &lhs : first_accesses) {
    for (const auto &rhs : second_accesses) {
      if (!lhs.writes && !rhs.writes) {
        continue;
      }
      auto lhs_root = GetPointerRoot(lhs.pointer);
      auto rhs_root = GetPointerRoot(rhs.pointer);
      if (RootsAreDistinct(lhs_root, rhs_root) ||
          (lhs_root.kind == RootKind::kParameter && rhs_root.kind == RootKind::kParameter &&
           AreProvenNoAliasParameters(lhs_root.value, rhs_root.value))) {
        continue;
      }
      if (lhs_root.value == nullptr || lhs_root.value != rhs_root.value) {
        return false;
      }
      if (StructurallyEqual(lhs.pointer, rhs.pointer, argument_map) &&
          HasDirectInductionIndex(lhs.pointer, rhs.pointer, first_iv_arg, second_iv_arg, argument_map)) {
        continue;
      }
      if (!AffineDependencesAllowFusion(lhs.pointer, rhs.pointer, first_iv_arg, second_iv_arg, argument_map)) {
        return false;
      }
    }
  }
  return true;
}

auto MLIRLoopFusionPass::Fuse(mlir::Block *block, size_t first_index, mlir::scf::WhileOp *first,
                              mlir::scf::WhileOp *second) -> void {
  auto *first_condition_block = first->GetCondRegion()->blocks_[0];
  auto *first_body = first->GetBodyRegion()->blocks_[0];
  auto *second_condition_block = second->GetCondRegion()->blocks_[0];
  auto *second_body = second->GetBodyRegion()->blocks_[0];
  auto *first_condition = mlir::DynamicOrNull<mlir::scf::ConditionOp>(first_condition_block->operations_.back());
  auto *second_condition = mlir::DynamicOrNull<mlir::scf::ConditionOp>(second_condition_block->operations_.back());
  auto *first_yield = mlir::DynamicOrNull<mlir::scf::YieldOp>(first_body->operations_.back());
  auto *second_yield = mlir::DynamicOrNull<mlir::scf::YieldOp>(second_body->operations_.back());

  auto carried = first_condition->GetOperands();
  auto original_values = first_condition->GetOriginalValues();
  auto second_carried = second_condition->GetOperands();
  auto second_original_values = second_condition->GetOriginalValues();
  carried.insert(carried.end(), second_carried.begin(), second_carried.end());
  original_values.insert(original_values.end(), second_original_values.begin(), second_original_values.end());
  first_condition->SetLoopCarriedValues(carried, original_values);

  first_condition_block->arguments_.insert(first_condition_block->arguments_.end(),
                                           second_condition_block->arguments_.begin(),
                                           second_condition_block->arguments_.end());
  first_body->arguments_.insert(first_body->arguments_.end(), second_body->arguments_.begin(),
                                second_body->arguments_.end());

  auto combined_yield = first_yield->GetOperands();
  auto &second_yield_values = second_yield->GetOperands();
  combined_yield.insert(combined_yield.end(), second_yield_values.begin(), second_yield_values.end());
  first_yield->operands_ = std::move(combined_yield);

  auto insertion = first_body->operations_.end() - 1;
  first_body->operations_.insert(insertion, second_body->operations_.begin(), second_body->operations_.end() - 1);

  for (size_t index = 0; index < second->GetResultsSize(); ++index) {
    auto *result = GET_MLIR_CONTEXT()->CreateValue();
    result->SetType(second->GetResultByIndex(index)->GetType());
    result->op_ = first;
    first->results_.emplace_back(result);
    second->GetResultByIndex(index)->ReplaceAllUsesWith(result);
  }

  block->operations_.erase(block->operations_.begin() + static_cast<std::ptrdiff_t>(first_index + 1));
}

}  // namespace middleend::pass::transform
