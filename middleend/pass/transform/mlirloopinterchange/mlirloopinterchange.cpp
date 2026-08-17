#include "middleend/pass/transform/mlirloopinterchange/mlirloopinterchange.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "context/compiler_context.h"
#include "context/mlir_context.h"
#include "middleend/mlir/dialect/arithm/binaryop.h"
#include "middleend/mlir/dialect/arithm/copy.h"
#include "middleend/mlir/dialect/arithm/unaryop.h"
#include "middleend/mlir/dialect/func/funccall.h"
#include "middleend/mlir/dialect/func/funcdef.h"
#include "middleend/mlir/dialect/func/return.h"
#include "middleend/mlir/dialect/scf/break.h"
#include "middleend/mlir/dialect/scf/condition.h"
#include "middleend/mlir/dialect/scf/continue.h"
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
#include "middleend/pass/analysis/mlirmemoryeffects/mlirmemoryeffects.h"
#include "middleend/pass/transform/mlirloopunroll/inductionvariable.h"

#define GET_MLIR_CONTEXT() compiler_context_->GetIRContext()->GetMLIRContext()

namespace middleend::pass::transform {
namespace {

struct LoopParts {
  mlir::Block *condition_block{nullptr};
  mlir::Block *body_block{nullptr};
  mlir::scf::ConditionOp *condition{nullptr};
  mlir::scf::YieldOp *yield{nullptr};
};

struct MemoryAccess {
  mlir::Value *pointer{nullptr};
  bool writes{false};
};

struct Affine2D {
  int64_t outer{0};
  int64_t inner{0};
  int64_t offset{0};
};

struct PointerAccess {
  std::vector<mlir::Value *> indices;
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
  lhs = TraceCopies(lhs);
  rhs = TraceCopies(rhs);
  auto *lhs_const = lhs == nullptr ? nullptr : mlir::DynamicOrNull<mlir::arithm::ConstOp>(lhs->op_);
  auto *rhs_const = rhs == nullptr ? nullptr : mlir::DynamicOrNull<mlir::arithm::ConstOp>(rhs->op_);
  return lhs_const != nullptr && rhs_const != nullptr && lhs_const->GetValue() == rhs_const->GetValue();
}

auto SameValue(mlir::Value *lhs, mlir::Value *rhs) -> bool {
  lhs = TraceCopies(lhs);
  rhs = TraceCopies(rhs);
  return lhs == rhs || SameConstant(lhs, rhs);
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

auto CheckedAffine(int64_t outer, int64_t inner, int64_t offset, Affine2D *result) -> bool {
  result->outer = outer;
  result->inner = inner;
  result->offset = offset;
  return true;
}

auto ExtractAffine(mlir::Value *value, mlir::Value *outer_iv, mlir::Value *inner_iv, Affine2D *result,
                   unsigned depth = 0) -> bool {
  if (depth > 32) {
    return false;
  }
  value = TraceCopies(value);
  outer_iv = TraceCopies(outer_iv);
  inner_iv = TraceCopies(inner_iv);
  if (value == outer_iv) {
    return CheckedAffine(1, 0, 0, result);
  }
  if (value == inner_iv) {
    return CheckedAffine(0, 1, 0, result);
  }
  int64_t constant = 0;
  if (GetConstantInt(value, &constant)) {
    return CheckedAffine(0, 0, constant, result);
  }
  if (value == nullptr || value->op_ == nullptr) {
    return false;
  }

  auto combine = [&](mlir::Value *lhs_value, mlir::Value *rhs_value, bool subtract) -> bool {
    Affine2D lhs;
    Affine2D rhs;
    if (!ExtractAffine(lhs_value, outer_iv, inner_iv, &lhs, depth + 1) ||
        !ExtractAffine(rhs_value, outer_iv, inner_iv, &rhs, depth + 1)) {
      return false;
    }
    __int128 outer = static_cast<__int128>(lhs.outer) +
                     (subtract ? -static_cast<__int128>(rhs.outer) : static_cast<__int128>(rhs.outer));
    __int128 inner = static_cast<__int128>(lhs.inner) +
                     (subtract ? -static_cast<__int128>(rhs.inner) : static_cast<__int128>(rhs.inner));
    __int128 offset = static_cast<__int128>(lhs.offset) +
                      (subtract ? -static_cast<__int128>(rhs.offset) : static_cast<__int128>(rhs.offset));
    if (outer < INT64_MIN || outer > INT64_MAX || inner < INT64_MIN || inner > INT64_MAX || offset < INT64_MIN ||
        offset > INT64_MAX) {
      return false;
    }
    return CheckedAffine(static_cast<int64_t>(outer), static_cast<int64_t>(inner), static_cast<int64_t>(offset),
                         result);
  };

  if (auto *add = mlir::DynamicOrNull<mlir::arithm::AddOp>(value->op_)) {
    return combine(add->GetLHS(), add->GetRHS(), false);
  }
  if (auto *sub = mlir::DynamicOrNull<mlir::arithm::SubOp>(value->op_)) {
    return combine(sub->GetLHS(), sub->GetRHS(), true);
  }
  if (auto *neg = mlir::DynamicOrNull<mlir::arithm::NegOp>(value->op_)) {
    Affine2D operand;
    if (!ExtractAffine(neg->GetOperand(), outer_iv, inner_iv, &operand, depth + 1) || operand.outer == INT64_MIN ||
        operand.inner == INT64_MIN || operand.offset == INT64_MIN) {
      return false;
    }
    return CheckedAffine(-operand.outer, -operand.inner, -operand.offset, result);
  }
  if (auto *mul = mlir::DynamicOrNull<mlir::arithm::MulOp>(value->op_)) {
    mlir::Value *affine_value = mul->GetLHS();
    mlir::Value *constant_value = mul->GetRHS();
    if (!GetConstantInt(constant_value, &constant)) {
      std::swap(affine_value, constant_value);
      if (!GetConstantInt(constant_value, &constant)) {
        return false;
      }
    }
    Affine2D affine;
    if (!ExtractAffine(affine_value, outer_iv, inner_iv, &affine, depth + 1)) {
      return false;
    }
    __int128 outer = static_cast<__int128>(affine.outer) * constant;
    __int128 inner = static_cast<__int128>(affine.inner) * constant;
    __int128 offset = static_cast<__int128>(affine.offset) * constant;
    if (outer < INT64_MIN || outer > INT64_MAX || inner < INT64_MIN || inner > INT64_MAX || offset < INT64_MIN ||
        offset > INT64_MAX) {
      return false;
    }
    return CheckedAffine(static_cast<int64_t>(outer), static_cast<int64_t>(inner), static_cast<int64_t>(offset),
                         result);
  }
  return false;
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
  if ((lhs != nullptr && mlir::DynamicOrNull<mlir::arithm::ConstOp>(lhs->op_) != nullptr) ||
      (rhs != nullptr && mlir::DynamicOrNull<mlir::arithm::ConstOp>(rhs->op_) != nullptr)) {
    return false;
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

auto CollectPointerIndices(mlir::Value *pointer, PointerAccess *access) -> void {
  pointer = TraceCopies(pointer);
  auto *get_ptr = pointer == nullptr ? nullptr : mlir::DynamicOrNull<mlir::var::VarGetPtrOp>(pointer->op_);
  if (get_ptr == nullptr) {
    return;
  }
  CollectPointerIndices(get_ptr->GetPtr(), access);
  auto indices = get_ptr->GetIndices();
  access->indices.insert(access->indices.end(), indices.begin(), indices.end());
}

auto GetLoopParts(mlir::scf::WhileOp *loop, LoopParts *parts) -> bool {
  if (loop == nullptr || loop->GetCondRegion()->blocks_.size() != 1 || loop->GetBodyRegion()->blocks_.size() != 1) {
    return false;
  }
  parts->condition_block = loop->GetCondRegion()->blocks_[0];
  parts->body_block = loop->GetBodyRegion()->blocks_[0];
  if (parts->condition_block->operations_.empty() || parts->body_block->operations_.empty()) {
    return false;
  }
  parts->condition = mlir::DynamicOrNull<mlir::scf::ConditionOp>(parts->condition_block->operations_.back());
  parts->yield = mlir::DynamicOrNull<mlir::scf::YieldOp>(parts->body_block->operations_.back());
  return parts->condition != nullptr && parts->yield != nullptr &&
         parts->condition_block->arguments_.size() == parts->body_block->arguments_.size() &&
         parts->condition_block->arguments_.size() == parts->condition->GetOperands().size() &&
         parts->condition_block->arguments_.size() == parts->condition->GetOriginalValues().size() &&
         parts->condition_block->arguments_.size() == parts->yield->GetOperands().size();
}

auto BodyEscapesOwnLoop(mlir::Block *body) -> bool {
  for (auto *operation : body->operations_) {
    if (mlir::DynamicOrNull<mlir::scf::BreakOp>(operation) != nullptr ||
        mlir::DynamicOrNull<mlir::scf::ContinueOp>(operation) != nullptr ||
        mlir::DynamicOrNull<mlir::func::ReturnOp>(operation) != nullptr) {
      return true;
    }
    if (auto *if_op = mlir::DynamicOrNull<mlir::scf::IfOp>(operation)) {
      for (auto *region : {if_op->GetThenRegion(), if_op->GetElseRegion()}) {
        if (region != nullptr && !region->blocks_.empty() && BodyEscapesOwnLoop(region->blocks_[0])) {
          return true;
        }
      }
    }
  }
  return false;
}

auto IsGlueOperation(mlir::Operation *operation) -> bool {
  return mlir::DynamicOrNull<mlir::arithm::ConstOp>(operation) != nullptr ||
         mlir::DynamicOrNull<mlir::arithm::CopyOp>(operation) != nullptr ||
         mlir::DynamicOrNull<mlir::arithm::CastOp>(operation) != nullptr ||
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

auto FindDirectInnerLoop(const LoopParts &outer_parts, mlir::scf::WhileOp **inner, size_t *inner_index) -> bool {
  *inner = nullptr;
  *inner_index = 0;
  auto &operations = outer_parts.body_block->operations_;
  if (operations.size() < 2 || operations.back() != outer_parts.yield) {
    return false;
  }
  for (size_t index = 0; index + 1 < operations.size(); ++index) {
    auto *candidate = mlir::DynamicOrNull<mlir::scf::WhileOp>(operations[index]);
    if (candidate == nullptr) {
      if (!IsGlueOperation(operations[index])) {
        return false;
      }
      continue;
    }
    if (*inner != nullptr) {
      return false;
    }
    *inner = candidate;
    *inner_index = index;
  }
  return *inner != nullptr;
}

auto ResultIndexForInner(mlir::Value *inner_result, const LoopParts &outer_parts) -> int {
  for (size_t index = 0; index < outer_parts.yield->GetOperands().size(); ++index) {
    if (SameValue(outer_parts.yield->GetOperands()[index], inner_result)) {
      return static_cast<int>(index);
    }
  }
  return -1;
}

auto InnerArgIsRead(mlir::Value *arg, mlir::Block *body) -> bool {
  for (auto *operation : body->operations_) {
    for (auto *operand : operation->operands_) {
      if (TraceCopies(operand) == arg) {
        return true;
      }
    }
    if (auto *condition = mlir::DynamicOrNull<mlir::scf::ConditionOp>(operation)) {
      for (auto *original : condition->GetOriginalValues()) {
        if (TraceCopies(original) == arg) {
          return true;
        }
      }
    }
    for (auto *region : operation->regions_) {
      for (auto *nested : region->blocks_) {
        if (InnerArgIsRead(arg, nested)) {
          return true;
        }
      }
    }
  }
  return false;
}

auto BuildInnerToOuterMap(mlir::scf::WhileOp *inner, const LoopParts &outer_parts, const LoopParts &inner_parts,
                          const loopiv::Induction &inner_iv, int outer_slot_for_inner_iv,
                          std::vector<int> *inner_to_outer) -> bool {
  const size_t inner_args = inner_parts.condition_block->arguments_.size();
  const size_t outer_args = outer_parts.condition_block->arguments_.size();
  inner_to_outer->assign(inner_args, -1);
  (*inner_to_outer)[inner_iv.index_] = outer_slot_for_inner_iv;

  std::unordered_set<int> used_outer_slots{outer_slot_for_inner_iv};
  for (size_t inner_index = 0; inner_index < inner_args; ++inner_index) {
    if (inner_index == inner_iv.index_) {
      continue;
    }
    int mapped = ResultIndexForInner(inner->GetResultByIndex(inner_index), outer_parts);
    if (mapped < 0) {
      for (size_t outer_index = 0; outer_index < outer_args; ++outer_index) {
        if (SameValue(inner_parts.condition->GetOriginalValues()[inner_index],
                      outer_parts.body_block->arguments_[outer_index]) ||
            SameValue(inner_parts.condition->GetOriginalValues()[inner_index],
                      outer_parts.condition_block->arguments_[outer_index])) {
          mapped = static_cast<int>(outer_index);
          break;
        }
      }
    }
    if (mapped < 0) {
      if (InnerArgIsRead(inner_parts.body_block->arguments_[inner_index], inner_parts.body_block)) {
        return false;
      }
      continue;
    }
    if (mapped >= static_cast<int>(outer_args) || used_outer_slots.count(mapped) > 0) {
      return false;
    }
    used_outer_slots.insert(mapped);
    (*inner_to_outer)[inner_index] = mapped;
  }
  return true;
}

auto IsIntegerAddMulReduction(mlir::Value *next, mlir::Value *arg) -> bool {
  if (arg == nullptr || arg->GetType() == nullptr || arg->GetType()->GetKind() != type::TypeKind::INT) {
    return false;
  }
  next = TraceCopies(next);
  arg = TraceCopies(arg);
  auto operand_is_arg = [&](mlir::Value *value) { return TraceCopies(value) == arg; };
  if (auto *add = next == nullptr ? nullptr : mlir::DynamicOrNull<mlir::arithm::AddOp>(next->op_)) {
    return operand_is_arg(add->GetLHS()) || operand_is_arg(add->GetRHS());
  }
  if (auto *mul = next == nullptr ? nullptr : mlir::DynamicOrNull<mlir::arithm::MulOp>(next->op_)) {
    return operand_is_arg(mul->GetLHS()) || operand_is_arg(mul->GetRHS());
  }
  return false;
}

auto ValidateScalarAndOuterYields(mlir::scf::WhileOp *inner, const LoopParts &outer_parts, const LoopParts &inner_parts,
                                  const loopiv::Induction &outer_iv, const loopiv::Induction &inner_iv,
                                  int outer_slot_for_inner_iv, const std::vector<int> &inner_to_outer) -> bool {
  std::vector<int> outer_to_inner(outer_parts.condition_block->arguments_.size(), -1);
  for (size_t inner_index = 0; inner_index < inner_to_outer.size(); ++inner_index) {
    int outer_index = inner_to_outer[inner_index];
    if (outer_index >= 0) {
      outer_to_inner[outer_index] = static_cast<int>(inner_index);
    }
  }

  for (size_t inner_index = 0; inner_index < inner_to_outer.size(); ++inner_index) {
    if (inner_index == inner_iv.index_ || inner_to_outer[inner_index] < 0) {
      continue;
    }
    auto *arg = inner_parts.body_block->arguments_[inner_index];
    auto *next = inner_parts.yield->GetOperands()[inner_index];
    if (!SameValue(next, arg) && !IsIntegerAddMulReduction(next, arg)) {
      return false;
    }
  }

  for (size_t outer_index = 0; outer_index < outer_parts.yield->GetOperands().size(); ++outer_index) {
    auto *yield = outer_parts.yield->GetOperands()[outer_index];
    if (outer_index == outer_iv.index_) {
      continue;
    }
    if (static_cast<int>(outer_index) == outer_slot_for_inner_iv) {
      if (!SameValue(yield, inner->GetResultByIndex(inner_iv.index_))) {
        return false;
      }
      continue;
    }
    int inner_index = outer_to_inner[outer_index];
    if (inner_index >= 0) {
      if (!SameValue(yield, inner->GetResultByIndex(static_cast<size_t>(inner_index)))) {
        return false;
      }
      continue;
    }
    if (!SameValue(yield, outer_parts.body_block->arguments_[outer_index])) {
      return false;
    }
  }
  return true;
}

auto CollectMemoryAccesses(mlir::Operation *operation, const analysis::MLIRMemoryEffectAnalysis *memory_effects,
                           std::vector<MemoryAccess> *accesses) -> bool {
  if (auto *load = mlir::DynamicOrNull<mlir::var::VarGetValOp>(operation)) {
    accesses->push_back({load->GetPtr(), false});
  } else if (auto *store = mlir::DynamicOrNull<mlir::var::VarSetOp>(operation)) {
    accesses->push_back({store->GetPtr(), true});
  } else if (auto *call = mlir::DynamicOrNull<mlir::func::FuncCallOp>(operation)) {
    if (!memory_effects->IsReadNoneAndIOClean(call)) {
      return false;
    }
  }
  for (auto *region : operation->regions_) {
    for (auto *block : region->blocks_) {
      for (auto *nested : block->operations_) {
        if (!CollectMemoryAccesses(nested, memory_effects, accesses)) {
          return false;
        }
      }
    }
  }
  return true;
}

auto CollectLoopMemoryAccesses(const LoopParts &inner_parts, const analysis::MLIRMemoryEffectAnalysis *memory_effects,
                               std::vector<MemoryAccess> *accesses) -> bool {
  for (size_t index = 0; index + 1 < inner_parts.body_block->operations_.size(); ++index) {
    if (!CollectMemoryAccesses(inner_parts.body_block->operations_[index], memory_effects, accesses)) {
      return false;
    }
  }
  return true;
}

auto ConstantDifferentDimension(mlir::Value *lhs, mlir::Value *rhs, mlir::Value *outer_iv,
                                mlir::Value *inner_iv) -> bool {
  Affine2D lhs_affine;
  Affine2D rhs_affine;
  if (!ExtractAffine(lhs, outer_iv, inner_iv, &lhs_affine) || !ExtractAffine(rhs, outer_iv, inner_iv, &rhs_affine)) {
    return false;
  }
  return lhs_affine.outer == 0 && lhs_affine.inner == 0 && rhs_affine.outer == 0 && rhs_affine.inner == 0 &&
         lhs_affine.offset != rhs_affine.offset;
}

auto Rank2Injective(const std::vector<mlir::Value *> &indices, mlir::Value *outer_iv, mlir::Value *inner_iv) -> bool {
  std::vector<std::array<int64_t, 2>> rows;
  for (auto *index : indices) {
    Affine2D affine;
    if (!ExtractAffine(index, outer_iv, inner_iv, &affine)) {
      return false;
    }
    if (affine.outer != 0 || affine.inner != 0) {
      rows.push_back({affine.outer, affine.inner});
    }
  }
  for (size_t lhs = 0; lhs < rows.size(); ++lhs) {
    for (size_t rhs = lhs + 1; rhs < rows.size(); ++rhs) {
      __int128 determinant =
          static_cast<__int128>(rows[lhs][0]) * rows[rhs][1] - static_cast<__int128>(rows[lhs][1]) * rows[rhs][0];
      if (determinant != 0) {
        return true;
      }
    }
  }
  return false;
}

auto AccessesAreSameIterationOrNoAlias(mlir::Value *lhs_pointer, mlir::Value *rhs_pointer, mlir::Value *outer_iv,
                                       mlir::Value *inner_iv) -> bool {
  PointerAccess lhs;
  PointerAccess rhs;
  CollectPointerIndices(lhs_pointer, &lhs);
  CollectPointerIndices(rhs_pointer, &rhs);
  if (lhs.indices.empty() || lhs.indices.size() != rhs.indices.size()) {
    return false;
  }
  for (size_t index = 0; index < lhs.indices.size(); ++index) {
    if (ConstantDifferentDimension(lhs.indices[index], rhs.indices[index], outer_iv, inner_iv)) {
      return true;
    }
  }

  const std::unordered_map<mlir::Value *, mlir::Value *> no_substitutions;
  for (size_t index = 0; index < lhs.indices.size(); ++index) {
    if (!StructurallyEqual(lhs.indices[index], rhs.indices[index], no_substitutions)) {
      return false;
    }
  }
  return Rank2Injective(lhs.indices, outer_iv, inner_iv);
}

auto DependencesAllowInterchange(const std::vector<MemoryAccess> &accesses, mlir::Value *outer_iv,
                                 mlir::Value *inner_iv) -> bool {
  for (size_t lhs_index = 0; lhs_index < accesses.size(); ++lhs_index) {
    for (size_t rhs_index = lhs_index; rhs_index < accesses.size(); ++rhs_index) {
      const auto &lhs = accesses[lhs_index];
      const auto &rhs = accesses[rhs_index];
      if (!lhs.writes && !rhs.writes) {
        continue;
      }
      auto lhs_root = analysis::MLIRMemoryEffectAnalysis::GetPointerRoot(lhs.pointer);
      auto rhs_root = analysis::MLIRMemoryEffectAnalysis::GetPointerRoot(rhs.pointer);
      if (analysis::MLIRMemoryEffectAnalysis::RootsAreDistinct(lhs_root, rhs_root)) {
        continue;
      }
      if (lhs_root.value == nullptr || lhs_root.value != rhs_root.value) {
        return false;
      }
      if (!AccessesAreSameIterationOrNoAlias(lhs.pointer, rhs.pointer, outer_iv, inner_iv)) {
        return false;
      }
    }
  }
  return true;
}

auto LocalityScore(const std::vector<MemoryAccess> &accesses, mlir::Value *candidate_inner_iv) -> int {
  int score = 0;
  for (const auto &access : accesses) {
    PointerAccess pointer_access;
    CollectPointerIndices(access.pointer, &pointer_access);
    if (pointer_access.indices.empty()) {
      continue;
    }
    auto *last = TraceCopies(pointer_access.indices.back());
    candidate_inner_iv = TraceCopies(candidate_inner_iv);
    if (last == candidate_inner_iv) {
      score += access.writes ? 4 : 2;
      continue;
    }
    if (last != nullptr && last->op_ != nullptr) {
      Affine2D affine;
      if (ExtractAffine(last, candidate_inner_iv, nullptr, &affine) && affine.outer != 0) {
        score += access.writes ? 3 : 1;
      }
    }
  }
  return score;
}

auto MakeStepValue(context::MLIRContext *context, mlir::Block *block, mlir::Value *current,
                   const loopiv::Induction &iv) -> mlir::Value * {
  auto *constant = context->CreateOperation<mlir::arithm::ConstOp>(context, static_cast<int>(iv.step_));
  constant->GetRes()->SetType(current->GetType());
  block->AddOperation(constant);
  if (iv.step_kind_ == loopiv::StepKind::kAdd) {
    auto *add = context->CreateOperation<mlir::arithm::AddOp>(context, current, constant->GetRes());
    add->GetRes()->SetType(current->GetType());
    block->AddOperation(add);
    return add->GetRes();
  }
  auto *mul = context->CreateOperation<mlir::arithm::MulOp>(context, current, constant->GetRes());
  mul->GetRes()->SetType(current->GetType());
  block->AddOperation(mul);
  return mul->GetRes();
}

auto CloneConditionValue(mlir::utils::OperationCloner *cloner, mlir::Block *source_condition_block,
                         mlir::scf::ConditionOp *source_condition,
                         mlir::Block *target_condition_block) -> mlir::Value * {
  for (auto *operation : source_condition_block->operations_) {
    if (operation == source_condition) {
      break;
    }
    target_condition_block->AddOperation(cloner->Clone(operation));
  }
  return cloner->RemapValue(source_condition->GetCondition());
}

}  // namespace

auto MLIRLoopInterchangePass::Run() -> void {
  PreRun();
  if (std::getenv("LOOP_INTERCHANGE_OFF") != nullptr) {
    return;
  }
  memory_effects_ =
      compiler_context_->GetPassContext()->GetSpecPass<analysis::MLIRMemoryEffectAnalysis>("MLIRMemoryEffectAnalysis");
  if (memory_effects_ == nullptr) {
    throw std::logic_error("MLIRLoopInterchangePass requires MLIRMemoryEffectAnalysis");
  }
  candidates_ = 0;
  interchanged_ = 0;
  shape_rejects_ = 0;
  domain_rejects_ = 0;
  dependence_rejects_ = 0;
  profitability_rejects_ = 0;

  auto *module = GET_MLIR_CONTEXT()->GetModuleOp();
  if (module == nullptr) {
    throw std::logic_error("Module operation is not set in MLIRContext");
  }
  for (auto *operation : module->GetBlock()->operations_) {
    auto *function = mlir::DynamicOrNull<mlir::func::FuncDefOp>(operation);
    if (function != nullptr && !mlir::func::IsLibraryFunction(function->GetName())) {
      RunOnFunction(function);
    }
  }
  if (std::getenv("LOOP_OPT_STATS") != nullptr) {
    std::cerr << "LoopInterchange: candidates=" << candidates_ << " interchanged=" << interchanged_
              << " shape=" << shape_rejects_ << " domain=" << domain_rejects_ << " dependence=" << dependence_rejects_
              << " profitability=" << profitability_rejects_ << '\n';
  }
}

auto MLIRLoopInterchangePass::RunOnFunction(mlir::func::FuncDefOp *func_op) -> bool {
  bool changed = false;
  bool iteration_changed = false;
  do {
    mlir::utils::RebuildFunctionUses(func_op);
    iteration_changed = InterchangeInBlock(func_op->GetEntryBlock());
    changed = changed || iteration_changed;
  } while (iteration_changed);
  if (changed) {
    mlir::utils::RebuildFunctionUses(func_op);
  }
  return changed;
}

auto MLIRLoopInterchangePass::InterchangeInBlock(mlir::Block *block) -> bool {
  bool changed = false;
  std::vector<mlir::Operation *> original = block->operations_;
  for (auto *operation : original) {
    for (auto *region : operation->regions_) {
      for (auto *nested : region->blocks_) {
        changed = InterchangeInBlock(nested) || changed;
      }
    }
  }

  for (size_t index = 0; index < block->operations_.size(); ++index) {
    if (mlir::DynamicOrNull<mlir::scf::WhileOp>(block->operations_[index]) == nullptr) {
      continue;
    }
    if (TryInterchange(block, index)) {
      changed = true;
    }
  }
  return changed;
}

auto MLIRLoopInterchangePass::TryInterchange(mlir::Block *parent, size_t index) -> bool {
  auto *outer = mlir::DynamicOrNull<mlir::scf::WhileOp>(parent->operations_[index]);
  if (outer == nullptr) {
    return false;
  }

  LoopParts outer_parts;
  if (!GetLoopParts(outer, &outer_parts) || BodyEscapesOwnLoop(outer_parts.body_block)) {
    ++shape_rejects_;
    return false;
  }
  mlir::scf::WhileOp *inner = nullptr;
  size_t ignored_inner_index = 0;
  if (!FindDirectInnerLoop(outer_parts, &inner, &ignored_inner_index)) {
    return false;
  }
  LoopParts inner_parts;
  if (!GetLoopParts(inner, &inner_parts) || BodyEscapesOwnLoop(inner_parts.body_block)) {
    ++shape_rejects_;
    return false;
  }

  auto outer_iv = loopiv::Recognize(outer);
  auto inner_iv = loopiv::Recognize(inner);
  if (!outer_iv.valid_ || !inner_iv.valid_ || outer_iv.step_kind_ != loopiv::StepKind::kAdd ||
      inner_iv.step_kind_ != loopiv::StepKind::kAdd || outer_iv.step_ <= 0 || inner_iv.step_ <= 0 ||
      (outer_iv.kind_ != loopiv::CmpKind::kLT && outer_iv.kind_ != loopiv::CmpKind::kLE) ||
      (inner_iv.kind_ != loopiv::CmpKind::kLT && inner_iv.kind_ != loopiv::CmpKind::kLE)) {
    ++domain_rejects_;
    return false;
  }
  if (loopiv::IsDefinedInLoop(TraceCopies(outer_iv.bound_), outer) ||
      loopiv::IsDefinedInLoop(TraceCopies(inner_iv.init_), outer) ||
      loopiv::IsDefinedInLoop(TraceCopies(inner_iv.bound_), outer)) {
    ++domain_rejects_;
    return false;
  }

  int outer_slot_for_inner_iv = ResultIndexForInner(inner->GetResultByIndex(inner_iv.index_), outer_parts);
  if (outer_slot_for_inner_iv < 0 || static_cast<size_t>(outer_slot_for_inner_iv) == outer_iv.index_) {
    ++shape_rejects_;
    return false;
  }
  std::vector<int> inner_to_outer;
  if (!BuildInnerToOuterMap(inner, outer_parts, inner_parts, inner_iv, outer_slot_for_inner_iv, &inner_to_outer) ||
      !ValidateScalarAndOuterYields(inner, outer_parts, inner_parts, outer_iv, inner_iv, outer_slot_for_inner_iv,
                                    inner_to_outer)) {
    ++shape_rejects_;
    return false;
  }

  std::vector<MemoryAccess> accesses;
  if (!CollectLoopMemoryAccesses(inner_parts, memory_effects_, &accesses)) {
    ++dependence_rejects_;
    return false;
  }
  auto *original_outer_iv_value = outer_parts.body_block->arguments_[outer_iv.index_];
  auto *original_inner_iv_value = inner_parts.body_block->arguments_[inner_iv.index_];
  ++candidates_;
  if (!DependencesAllowInterchange(accesses, original_outer_iv_value, original_inner_iv_value)) {
    ++dependence_rejects_;
    return false;
  }
  int original_score = LocalityScore(accesses, original_inner_iv_value);
  int interchanged_score = LocalityScore(accesses, original_outer_iv_value);
  if (accesses.empty() || interchanged_score <= original_score) {
    ++profitability_rejects_;
    return false;
  }

  auto *context = GET_MLIR_CONTEXT();
  const size_t slots = outer_parts.condition_block->arguments_.size();
  std::vector<int> outer_to_inner(slots, -1);
  for (size_t inner_arg = 0; inner_arg < inner_to_outer.size(); ++inner_arg) {
    if (inner_to_outer[inner_arg] >= 0) {
      outer_to_inner[static_cast<size_t>(inner_to_outer[inner_arg])] = static_cast<int>(inner_arg);
    }
  }

  // New outer loop carries the same result slots as the original outer loop,
  // but its induction slot is the original inner induction variable.
  auto *new_outer_cond_block = new mlir::Block();
  auto *new_outer_body_block = new mlir::Block();
  std::vector<mlir::Value *> new_outer_args;
  new_outer_args.reserve(slots);
  for (size_t slot = 0; slot < slots; ++slot) {
    auto *arg = context->CreateValue();
    arg->SetType(outer_parts.condition_block->arguments_[slot]->GetType());
    new_outer_args.push_back(arg);
    new_outer_cond_block->AddArgument(arg);
    new_outer_body_block->AddArgument(arg);
  }

  cloner_.value_map_.clear();
  for (size_t inner_arg = 0; inner_arg < inner_parts.condition_block->arguments_.size(); ++inner_arg) {
    int outer_slot = inner_to_outer[inner_arg];
    if (outer_slot >= 0) {
      cloner_.value_map_[inner_parts.condition_block->arguments_[inner_arg]] =
          new_outer_args[static_cast<size_t>(outer_slot)];
    }
  }
  auto *new_outer_cond_value =
      CloneConditionValue(&cloner_, inner_parts.condition_block, inner_parts.condition, new_outer_cond_block);
  auto *new_outer_condition = context->CreateOperation<mlir::scf::ConditionOp>(
      context, new_outer_cond_value, std::unordered_map<symboltable::VarEntry *, mlir::Value *>{});
  std::vector<mlir::Value *> new_outer_originals;
  new_outer_originals.reserve(slots);
  auto outer_originals = outer_parts.condition->GetOriginalValues();
  for (size_t slot = 0; slot < slots; ++slot) {
    if (static_cast<int>(slot) == outer_slot_for_inner_iv) {
      new_outer_originals.push_back(TraceCopies(inner_iv.init_));
    } else {
      new_outer_originals.push_back(outer_originals[slot]);
    }
  }
  new_outer_condition->SetLoopCarriedValues(new_outer_args, new_outer_originals);
  new_outer_cond_block->AddOperation(new_outer_condition);

  // New inner loop carries the original outer loop state for one fixed original
  // inner-IV value.
  auto *new_inner_cond_block = new mlir::Block();
  auto *new_inner_body_block = new mlir::Block();
  std::vector<mlir::Value *> new_inner_args;
  new_inner_args.reserve(slots);
  for (size_t slot = 0; slot < slots; ++slot) {
    auto *arg = context->CreateValue();
    arg->SetType(outer_parts.condition_block->arguments_[slot]->GetType());
    new_inner_args.push_back(arg);
    new_inner_cond_block->AddArgument(arg);
    new_inner_body_block->AddArgument(arg);
  }

  cloner_.value_map_.clear();
  for (size_t slot = 0; slot < slots; ++slot) {
    cloner_.value_map_[outer_parts.condition_block->arguments_[slot]] = new_inner_args[slot];
  }
  auto *new_inner_cond_value =
      CloneConditionValue(&cloner_, outer_parts.condition_block, outer_parts.condition, new_inner_cond_block);
  auto *new_inner_condition = context->CreateOperation<mlir::scf::ConditionOp>(
      context, new_inner_cond_value, std::unordered_map<symboltable::VarEntry *, mlir::Value *>{});
  std::vector<mlir::Value *> new_inner_originals;
  new_inner_originals.reserve(slots);
  for (size_t slot = 0; slot < slots; ++slot) {
    if (slot == outer_iv.index_) {
      new_inner_originals.push_back(outer_originals[slot]);
    } else {
      new_inner_originals.push_back(new_outer_args[slot]);
    }
  }
  new_inner_condition->SetLoopCarriedValues(new_inner_args, new_inner_originals);
  new_inner_cond_block->AddOperation(new_inner_condition);

  cloner_.value_map_.clear();
  for (size_t slot = 0; slot < slots; ++slot) {
    cloner_.value_map_[outer_parts.condition_block->arguments_[slot]] = new_inner_args[slot];
    cloner_.value_map_[outer_parts.body_block->arguments_[slot]] = new_inner_args[slot];
  }
  for (size_t inner_arg = 0; inner_arg < inner_parts.condition_block->arguments_.size(); ++inner_arg) {
    int outer_slot = inner_to_outer[inner_arg];
    if (outer_slot < 0) {
      continue;
    }
    mlir::Value *replacement = nullptr;
    if (inner_arg == inner_iv.index_) {
      replacement = new_outer_args[static_cast<size_t>(outer_slot_for_inner_iv)];
    } else {
      replacement = new_inner_args[static_cast<size_t>(outer_slot)];
    }
    cloner_.value_map_[inner_parts.condition_block->arguments_[inner_arg]] = replacement;
    cloner_.value_map_[inner_parts.body_block->arguments_[inner_arg]] = replacement;
  }
  for (size_t op_index = 0; op_index + 1 < inner_parts.body_block->operations_.size(); ++op_index) {
    new_inner_body_block->AddOperation(cloner_.Clone(inner_parts.body_block->operations_[op_index]));
  }
  auto *outer_step = MakeStepValue(context, new_inner_body_block, new_inner_args[outer_iv.index_], outer_iv);

  std::vector<mlir::Value *> new_inner_yields;
  new_inner_yields.reserve(slots);
  for (size_t slot = 0; slot < slots; ++slot) {
    if (slot == outer_iv.index_) {
      new_inner_yields.push_back(outer_step);
      continue;
    }
    if (static_cast<int>(slot) == outer_slot_for_inner_iv) {
      new_inner_yields.push_back(new_inner_args[slot]);
      continue;
    }
    int inner_arg = outer_to_inner[slot];
    if (inner_arg >= 0) {
      new_inner_yields.push_back(cloner_.RemapValue(inner_parts.yield->GetOperands()[static_cast<size_t>(inner_arg)]));
    } else {
      new_inner_yields.push_back(new_inner_args[slot]);
    }
  }
  new_inner_body_block->AddOperation(context->CreateOperation<mlir::scf::YieldOp>(context, new_inner_yields));

  auto *new_inner_cond_region = new mlir::Region();
  new_inner_cond_region->AddBlock(new_inner_cond_block);
  auto *new_inner_body_region = new mlir::Region();
  new_inner_body_region->AddBlock(new_inner_body_block);
  auto *new_inner =
      context->CreateOperation<mlir::scf::WhileOp>(context, slots, new_inner_cond_region, new_inner_body_region);
  for (size_t slot = 0; slot < slots; ++slot) {
    new_inner->GetResultByIndex(slot)->SetType(outer->GetResultByIndex(slot)->GetType());
  }
  new_outer_body_block->AddOperation(new_inner);

  auto *inner_step = MakeStepValue(context, new_outer_body_block,
                                   new_outer_args[static_cast<size_t>(outer_slot_for_inner_iv)], inner_iv);
  std::vector<mlir::Value *> new_outer_yields;
  new_outer_yields.reserve(slots);
  for (size_t slot = 0; slot < slots; ++slot) {
    if (static_cast<int>(slot) == outer_slot_for_inner_iv) {
      new_outer_yields.push_back(inner_step);
    } else {
      new_outer_yields.push_back(new_inner->GetResultByIndex(slot));
    }
  }
  new_outer_body_block->AddOperation(context->CreateOperation<mlir::scf::YieldOp>(context, new_outer_yields));

  auto *new_outer_cond_region = new mlir::Region();
  new_outer_cond_region->AddBlock(new_outer_cond_block);
  auto *new_outer_body_region = new mlir::Region();
  new_outer_body_region->AddBlock(new_outer_body_block);
  auto *new_outer =
      context->CreateOperation<mlir::scf::WhileOp>(context, slots, new_outer_cond_region, new_outer_body_region);
  for (size_t slot = 0; slot < slots; ++slot) {
    new_outer->GetResultByIndex(slot)->SetType(outer->GetResultByIndex(slot)->GetType());
    outer->GetResultByIndex(slot)->ReplaceAllUsesWith(new_outer->GetResultByIndex(slot));
  }
  parent->operations_[index] = new_outer;
  ++interchanged_;
  return true;
}

}  // namespace middleend::pass::transform
