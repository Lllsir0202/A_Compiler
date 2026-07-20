#include "middleend/pass/analysis/alias/alias.h"
#include <stdexcept>
#include "context/compiler_context.h"
#include "middleend/pass/analysis/scev/scevpass.h"
#include "middleend/pass/analysis/sideeffect/sideeffect.h"
#include "middleend/mlir/dialect/base/copy.h"
#include "middleend/mlir/dialect/base/phi.h"
#include "middleend/mlir/dialect/base/funccall.h"
#include "middleend/mlir/dialect/func/funccall.h"
#include "middleend/mlir/dialect/base/funcdef.h"
#include "middleend/mlir/dialect/base/unaryop.h"
#include "middleend/mlir/dialect/func/return.h"
#include "middleend/mlir/dialect/var/def.h"
#include "middleend/mlir/dialect/var/getptr.h"
#include "middleend/mlir/dialect/var/globaldef.h"
#include "middleend/mlir/dialect/var/set.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/block.h"
#include "middleend/mlir/utils/value/value.h"
#include "middleend/pass/analysis/cfg/cfg.h"
#include "middleend/pass/analysis/cfg/mlircfg.h"

namespace middleend::pass::analysis {
namespace {
auto IsSameObject(const AliasAnalysisPass::MemoryObject &lhs, const AliasAnalysisPass::MemoryObject &rhs) -> bool {
  return lhs.kind == rhs.kind && lhs.root == rhs.root;
}
}  // namespace

auto AliasAnalysisPass::Run() -> void {
  PreRun();
  base_cache_.clear();
  param_values_.clear();
  escaped_locals_.clear();
  CollectParameters();
  ComputeEscape();
  side_effect_ = compiler_context_->GetPassContext()->GetSpecPass<FunctionSideEffectPass>("FunctionSideEffectPass");
  scev_ = compiler_context_->GetPassContext()->GetSpecPass<SCEVPass>("SCEVPass");
  Validate();
}

auto AliasAnalysisPass::CollectParameters() -> void {
  auto *cfg_pass = compiler_context_->GetPassContext()->GetSpecPass<CFGPass>("CFGPass");
  if (cfg_pass == nullptr) {
    throw std::logic_error("AliasAnalysisPass requires CFGPass");
  }
  for (auto *function_cfg : cfg_pass->Get()) {
    auto *mlir_cfg = dynamic_cast<MLIRFunctionCFG *>(function_cfg);
    if (mlir_cfg == nullptr || mlir_cfg->GetFuncOp() == nullptr) {
      continue;
    }
    for (auto *argument : mlir_cfg->GetFuncOp()->GetArguments()) {
      if (argument != nullptr) {
        param_values_.insert(argument);
      }
    }
  }
}

auto AliasAnalysisPass::ComputeEscape() -> void {
  auto *cfg_pass = compiler_context_->GetPassContext()->GetSpecPass<CFGPass>("CFGPass");
  if (cfg_pass == nullptr) {
    throw std::logic_error("AliasAnalysisPass requires CFGPass");
  }
  for (auto *function_cfg : cfg_pass->Get()) {
    auto *mlir_cfg = dynamic_cast<MLIRFunctionCFG *>(function_cfg);
    if (mlir_cfg == nullptr) {
      continue;
    }
    for (auto *cfg_node : mlir_cfg->GetNodes()) {
      auto *mlir_node = dynamic_cast<MLIRCFGNode *>(cfg_node);
      if (mlir_node == nullptr) {
        continue;
      }
      for (auto *operation : mlir_node->GetBlock()->operations_) {
        // A pointer escapes when it is passed to a call, returned, or stored into memory.
        if (mlir::DynamicOrNull<mlir::base::FuncCallOp>(operation) != nullptr ||
            mlir::DynamicOrNull<mlir::func::FuncCallOp>(operation) != nullptr) {
          for (auto *argument : operation->operands_) {
            MarkEscapeIfLocal(argument);
          }
          continue;
        }
        if (auto *store_op = mlir::DynamicOrNull<mlir::var::VarSetOp>(operation)) {
          MarkEscapeIfLocal(store_op->GetNewValue());
          continue;
        }
        if (auto *return_op = mlir::DynamicOrNull<mlir::func::ReturnOp>(operation)) {
          MarkEscapeIfLocal(return_op->GetReturnValue());
          continue;
        }
      }
    }
  }
}

auto AliasAnalysisPass::MarkEscapeIfLocal(mlir::Value *pointer) -> void {
  if (pointer == nullptr) {
    return;
  }
  auto object = GetBaseObject(pointer);
  if (object.kind == ObjectKind::Local) {
    escaped_locals_.insert(object.root);
  }
}

auto AliasAnalysisPass::GetBaseObject(mlir::Value *pointer) -> MemoryObject {
  auto cached = base_cache_.find(pointer);
  if (cached != base_cache_.end()) {
    return cached->second;
  }
  std::unordered_set<mlir::Value *> visiting;
  MemoryObject result = ResolveBaseObject(pointer, visiting);
  base_cache_[pointer] = result;
  return result;
}

auto AliasAnalysisPass::ResolveBaseObject(mlir::Value *pointer, std::unordered_set<mlir::Value *> &visiting)
    -> MemoryObject {
  mlir::Value *current = pointer;
  while (true) {
    if (current == nullptr) {
      return {ObjectKind::Unknown, nullptr};
    }
    auto *op = current->op_;
    if (op == nullptr) {
      return param_values_.count(current) > 0 ? MemoryObject{ObjectKind::Param, current}
                                              : MemoryObject{ObjectKind::Unknown, current};
    }
    if (auto *getptr_op = mlir::DynamicOrNull<mlir::var::VarGetPtrOp>(op)) {
      current = getptr_op->GetPtr();
      continue;
    }
    if (auto *copy_op = mlir::DynamicOrNull<mlir::base::CopyOp>(op)) {
      current = copy_op->GetSrc();
      continue;
    }
    if (mlir::DynamicOrNull<mlir::var::VarDefOp>(op) != nullptr) {
      return {ObjectKind::Local, current};
    }
    if (mlir::DynamicOrNull<mlir::var::GlobalVarDefOp>(op) != nullptr) {
      return {ObjectKind::Global, current};
    }
    // A pointer phi (e.g. a strength-reduced pointer induction variable) derives
    // from all of its incoming pointers. If every incoming that is not a cycle
    // back into this phi resolves to the same object, that is the base object;
    // otherwise it is genuinely unknown. Without this, such a phi would look like
    // an Unknown pointer and be wrongly declared distinct from a non-escaped local
    // it actually indexes into (see DefinitelyDistinct).
    if (auto *phi = mlir::DynamicOrNull<mlir::base::PhiOp>(op)) {
      if (!visiting.insert(current).second) {
        return {ObjectKind::Unknown, current};  // cycle back to a phi already on the stack
      }
      MemoryObject common{ObjectKind::Unknown, nullptr};
      bool have_common = false;
      for (auto *incoming : phi->GetIncomingValue()) {
        MemoryObject object = ResolveBaseObject(incoming, visiting);
        if (object.kind == ObjectKind::Unknown && object.root != nullptr && visiting.count(object.root) > 0) {
          continue;  // incoming folds back into a phi being resolved: not a fresh object
        }
        if (!have_common) {
          common = object;
          have_common = true;
        } else if (!IsSameObject(common, object)) {
          return {ObjectKind::Unknown, current};
        }
      }
      return have_common ? common : MemoryObject{ObjectKind::Unknown, current};
    }
    return {ObjectKind::Unknown, current};
  }
}

auto AliasAnalysisPass::Alias(mlir::Value *lhs, mlir::Value *rhs) -> AliasResult {
  if (lhs == nullptr || rhs == nullptr) {
    return AliasResult::MayAlias;
  }
  if (lhs == rhs) {
    return AliasResult::MustAlias;
  }
  auto lhs_object = GetBaseObject(lhs);
  auto rhs_object = GetBaseObject(rhs);
  if (lhs_object.kind != ObjectKind::Unknown && lhs_object.kind == rhs_object.kind &&
      lhs_object.root == rhs_object.root) {
    return SameObjectAlias(lhs, rhs);
  }
  if (DefinitelyDistinct(lhs_object, rhs_object)) {
    return AliasResult::NoAlias;
  }
  return AliasResult::MayAlias;
}

auto AliasAnalysisPass::DefinitelyDistinct(const MemoryObject &lhs, const MemoryObject &rhs) -> bool {
  // A non-escaped local cannot be named by anything except itself.
  if (lhs.kind == ObjectKind::Local && !IsEscaped(lhs) && !IsSameObject(lhs, rhs)) {
    return true;
  }
  if (rhs.kind == ObjectKind::Local && !IsEscaped(rhs) && !IsSameObject(lhs, rhs)) {
    return true;
  }
  // Two distinct named objects (local/global) are distinct storage.
  bool lhs_named = lhs.kind == ObjectKind::Local || lhs.kind == ObjectKind::Global;
  bool rhs_named = rhs.kind == ObjectKind::Local || rhs.kind == ObjectKind::Global;
  return lhs_named && rhs_named && !IsSameObject(lhs, rhs);
}

auto AliasAnalysisPass::SameObjectAlias(mlir::Value *lhs, mlir::Value *rhs) -> AliasResult {
  auto *lhs_getptr = lhs->op_ != nullptr ? mlir::DynamicOrNull<mlir::var::VarGetPtrOp>(lhs->op_) : nullptr;
  auto *rhs_getptr = rhs->op_ != nullptr ? mlir::DynamicOrNull<mlir::var::VarGetPtrOp>(rhs->op_) : nullptr;
  if (lhs_getptr == nullptr || rhs_getptr == nullptr || lhs_getptr->GetPtr() != rhs_getptr->GetPtr()) {
    return AliasResult::MayAlias;
  }
  auto lhs_indices = lhs_getptr->GetIndices();
  auto rhs_indices = rhs_getptr->GetIndices();
  if (lhs_indices.size() != rhs_indices.size()) {
    return AliasResult::MayAlias;
  }
  bool all_equal = true;
  for (size_t i = 0; i < lhs_indices.size(); ++i) {
    if (lhs_indices[i] == rhs_indices[i]) {
      continue;
    }
    all_equal = false;
    int lhs_constant = 0;
    int rhs_constant = 0;
    if (GetConstantIndex(lhs_indices[i], &lhs_constant) && GetConstantIndex(rhs_indices[i], &rhs_constant) &&
        lhs_constant != rhs_constant) {
      return AliasResult::NoAlias;
    }
    // Try SCEV: if the indices are provably different for every iteration, NoAlias.
    if (scev_ != nullptr && SCEVNoAlias(lhs_indices[i], rhs_indices[i])) {
      return AliasResult::NoAlias;
    }
  }
  return all_equal ? AliasResult::MustAlias : AliasResult::MayAlias;
}

auto AliasAnalysisPass::GetConstantIndex(mlir::Value *value, int *out) -> bool {
  if (value == nullptr || value->op_ == nullptr) {
    return false;
  }
  if (auto *const_int_op = mlir::DynamicOrNull<mlir::base::ConstIntOp>(value->op_)) {
    *out = const_int_op->GetValue();
    return true;
  }
  return false;
}

auto AliasAnalysisPass::SCEVNoAlias(mlir::Value *lhs_idx, mlir::Value *rhs_idx) -> bool {
  if (scev_ == nullptr || lhs_idx == nullptr || rhs_idx == nullptr) return false;

  const SCEV *lhs_scev = scev_->GetSCEV(lhs_idx);
  const SCEV *rhs_scev = scev_->GetSCEV(rhs_idx);

  // Same SCEV means the indices are identical for every iteration → MustAlias, not NoAlias.
  if (lhs_scev == rhs_scev) return false;

  // Case 1: Both are affine AddRecs on the same loop with the same step.
  //   lhs = {c1,+,s}, rhs = {c2,+,s} on the same loop
  //   The difference is the constant (c1-c2), independent of iteration.
  //   Under modular arithmetic: c1+i*s ≡ c2+i*s (mod 2^32) iff c1 ≡ c2.
  //   So if c1 != c2, they never alias — no no-wrap assumption needed.
  if (lhs_scev->GetKind() == SCEVKind::AddRec && rhs_scev->GetKind() == SCEVKind::AddRec) {
    const auto *lhs_ar = static_cast<const SCEVAddRecExpr *>(lhs_scev);
    const auto *rhs_ar = static_cast<const SCEVAddRecExpr *>(rhs_scev);
    if (lhs_ar->IsAffine() && rhs_ar->IsAffine() &&
        lhs_ar->GetLoop() == rhs_ar->GetLoop() &&
        lhs_ar->GetStep() == rhs_ar->GetStep()) {
      int lhs_start = 0, rhs_start = 0;
      const auto *lhs_start_scev = lhs_ar->GetStart();
      const auto *rhs_start_scev = rhs_ar->GetStart();
      if (lhs_start_scev->GetKind() == SCEVKind::Constant &&
          rhs_start_scev->GetKind() == SCEVKind::Constant) {
        lhs_start = static_cast<const SCEVConstant *>(lhs_start_scev)->GetInt();
        rhs_start = static_cast<const SCEVConstant *>(rhs_start_scev)->GetInt();
        if (lhs_start != rhs_start) return true;
      }
    }
  }

  // Case 2: The difference lhs - rhs is a compile-time non-zero constant.
  //   This handles AddRec vs Constant, AddRec vs AddRec with different steps,
  //   and any other pair where SCEV can fold the difference to a constant.
  const SCEV *diff = scev_->GetMinusSCEV(lhs_scev, rhs_scev);
  if (diff->GetKind() == SCEVKind::Constant) {
    const auto *diff_c = static_cast<const SCEVConstant *>(diff);
    if (diff_c->IsInt() && diff_c->GetInt() != 0) return true;
  }

  return false;
}

auto AliasAnalysisPass::CallCanNameObject(mlir::Operation *call, const MemoryObject &object) -> bool {
  if (object.kind != ObjectKind::Local || IsEscaped(object)) {
    return true;  // globals, params and escaped locals are reachable by other means
  }
  // Not escaped: the callee can only reach it through an argument of this call. SysY callees
  // cannot store the pointer (that is what makes it non-escaped), but they can freely read and
  // write through an argument for the duration of the call.
  for (auto *operand : call->operands_) {
    auto arg_object = GetBaseObject(operand);
    if (arg_object.kind != ObjectKind::Unknown && arg_object.root == object.root) {
      return true;
    }
  }
  return false;
}

auto AliasAnalysisPass::CallModRef(mlir::Operation *call, mlir::Value *pointer) -> bool {
  auto object = GetBaseObject(pointer);
  // A call may touch everything except an object it cannot even name.
  return CallCanNameObject(call, object);
}

auto AliasAnalysisPass::IsEscaped(const MemoryObject &object) -> bool {
  return object.kind == ObjectKind::Local && escaped_locals_.count(object.root) > 0;
}

auto AliasAnalysisPass::CallModRefPrecise(mlir::Operation *call, mlir::Value *pointer) -> bool {
  auto object = GetBaseObject(pointer);
  // Cheap check first: a call that cannot even name the object cannot touch it.
  if (!CallCanNameObject(call, object)) return false;

  // If we have inter-procedural side-effect info, use it.
  if (side_effect_ != nullptr) {
    std::string callee;
    if (auto *bc = mlir::DynamicOrNull<mlir::base::FuncCallOp>(call)) callee = bc->GetName();
    else if (auto *fc = mlir::DynamicOrNull<mlir::func::FuncCallOp>(call)) callee = fc->GetName();
    if (!callee.empty() && side_effect_->HasSummary(callee)) {
      const auto &s = side_effect_->GetSummary(callee);

      // Check memory-specific global effects.
      if (s.writes_all_globals || s.reads_all_globals) return true;
      if (object.kind == ObjectKind::Global && object.root != nullptr) {
        if (s.written_globals.count(object.root) > 0) return true;
        if (s.read_globals.count(object.root) > 0) return true;
      }

      // Check per-argument effects: if the callee reads or writes one of its
      // pointer arguments and `pointer` may alias that argument, the call
      // touches the location.
      const auto &arg_effects = s.arg_effects;
      if (!arg_effects.empty()) {
        for (size_t i = 0; i < arg_effects.size() && i < call->operands_.size(); ++i) {
          if (arg_effects[i] == SideEffectSummary::ArgEffect::kNone) continue;
          // The callee reads or writes operand i.
          auto arg_object = GetBaseObject(call->operands_[i]);
          // Same object and not provably non-overlapping.
          if (arg_object.kind != ObjectKind::Unknown && arg_object.root == object.root)
            return true;
          // Conservatively: MayAlias with an argument the callee touches.
          if (Alias(call->operands_[i], pointer) != AliasResult::NoAlias)
            return true;
        }
      }

      // Memory-only check: no memory read or write means the call is invisible
      // to the memory system (IO-only, like putint/getint).
      if (s.IsMemoryWriteNone() && s.IsMemoryReadNone() && arg_effects.empty())
        return false;
    }
  }

  // Conservative: call may touch any non-local memory.
  return true;
}

}  // namespace middleend::pass::analysis
