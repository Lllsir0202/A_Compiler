#include "middleend/pass/analysis/alias/alias.h"
#include <stdexcept>
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/base/copy.h"
#include "middleend/mlir/dialect/base/funccall.h"
#include "middleend/mlir/dialect/base/funcdef.h"
#include "middleend/mlir/dialect/base/unaryop.h"
#include "middleend/mlir/dialect/func/funccall.h"
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

  mlir::Value *current = pointer;
  MemoryObject result;
  while (true) {
    if (current == nullptr) {
      result = {ObjectKind::Unknown, nullptr};
      break;
    }
    auto *op = current->op_;
    if (op == nullptr) {
      result = param_values_.count(current) > 0 ? MemoryObject{ObjectKind::Param, current}
                                                : MemoryObject{ObjectKind::Unknown, current};
      break;
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
      result = {ObjectKind::Local, current};
      break;
    }
    if (mlir::DynamicOrNull<mlir::var::GlobalVarDefOp>(op) != nullptr) {
      result = {ObjectKind::Global, current};
      break;
    }
    result = {ObjectKind::Unknown, current};
    break;
  }
  base_cache_[pointer] = result;
  return result;
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

auto AliasAnalysisPass::CallModRef(mlir::Operation *call, mlir::Value *pointer) -> bool {
  (void)call;
  auto object = GetBaseObject(pointer);
  // A call may touch everything except a local whose address never escapes the function.
  bool is_non_escaped_local = object.kind == ObjectKind::Local && !IsEscaped(object);
  return !is_non_escaped_local;
}

auto AliasAnalysisPass::IsEscaped(const MemoryObject &object) -> bool {
  return object.kind == ObjectKind::Local && escaped_locals_.count(object.root) > 0;
}

}  // namespace middleend::pass::analysis
