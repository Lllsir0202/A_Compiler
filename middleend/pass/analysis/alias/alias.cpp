#include "middleend/pass/analysis/alias/alias.h"
#include <algorithm>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/base/copy.h"
#include "middleend/mlir/dialect/base/funccall.h"
#include "middleend/mlir/dialect/base/funcdef.h"
#include "middleend/mlir/dialect/base/phi.h"
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

struct DirectCall {
  std::string name;
  std::vector<mlir::Value *> arguments;
};

auto GetDirectCall(mlir::Operation *operation, DirectCall *out) -> bool {
  if (auto *call = mlir::DynamicOrNull<mlir::base::FuncCallOp>(operation)) {
    *out = {call->GetName(), call->GetOperands()};
    return true;
  }
  if (auto *call = mlir::DynamicOrNull<mlir::func::FuncCallOp>(operation)) {
    *out = {call->GetName(), call->GetOperands()};
    return true;
  }
  return false;
}
}  // namespace

auto AliasAnalysisPass::Run() -> void {
  PreRun();
  base_cache_.clear();
  param_values_.clear();
  escaped_locals_.clear();
  noalias_parameters_.clear();
  function_memory_summaries_.clear();
  CollectParameters();
  ComputeEscape();
  CollectInterproceduralFacts();
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

auto AliasAnalysisPass::CollectInterproceduralFacts() -> void {
  auto *cfg_pass = compiler_context_->GetPassContext()->GetSpecPass<CFGPass>("CFGPass");
  if (cfg_pass == nullptr) {
    throw std::logic_error("AliasAnalysisPass requires CFGPass");
  }

  std::unordered_map<std::string, mlir::base::FuncDefOp *> functions;
  std::unordered_map<std::string, std::vector<DirectCall>> calls_by_callee;
  std::unordered_map<std::string, std::vector<DirectCall>> calls_by_caller;
  for (auto *function_cfg : cfg_pass->Get()) {
    auto *mlir_cfg = dynamic_cast<MLIRFunctionCFG *>(function_cfg);
    if (mlir_cfg == nullptr || mlir_cfg->GetFuncOp() == nullptr) {
      continue;
    }
    auto *function = mlir_cfg->GetFuncOp();
    functions[function->GetName()] = function;
    auto &caller_calls = calls_by_caller[function->GetName()];
    for (auto *cfg_node : mlir_cfg->GetNodes()) {
      auto *mlir_node = dynamic_cast<MLIRCFGNode *>(cfg_node);
      if (mlir_node == nullptr) {
        continue;
      }
      for (auto *operation : mlir_node->GetBlock()->operations_) {
        DirectCall call;
        if (!GetDirectCall(operation, &call)) {
          continue;
        }
        caller_calls.emplace_back(call);
        calls_by_callee[call.name].emplace_back(std::move(call));
      }
    }
  }

  // SysY is compiled as a closed program with direct calls.  A pair of pointer
  // parameters is therefore no-alias when every call site supplies distinct
  // allocations. Iterate so a wrapper can inherit a fact already proved for
  // its own parameters; an unseeded recursive cycle never proves itself.
  bool noalias_changed = true;
  while (noalias_changed) {
    noalias_changed = false;
    for (const auto &[name, function] : functions) {
      auto parameters = function->GetArguments();
      auto call_iter = calls_by_callee.find(name);
      if (call_iter == calls_by_callee.end() || call_iter->second.empty()) {
        continue;
      }
      for (size_t lhs_index = 0; lhs_index < parameters.size(); ++lhs_index) {
        auto *lhs_parameter = parameters[lhs_index];
        if (lhs_parameter == nullptr || lhs_parameter->GetType() == nullptr ||
            lhs_parameter->GetType()->GetKind() != type::TypeKind::PTR) {
          continue;
        }
        for (size_t rhs_index = lhs_index + 1; rhs_index < parameters.size(); ++rhs_index) {
          auto *rhs_parameter = parameters[rhs_index];
          if (rhs_parameter == nullptr || rhs_parameter->GetType() == nullptr ||
              rhs_parameter->GetType()->GetKind() != type::TypeKind::PTR ||
              AreProvenNoAliasParameters(lhs_parameter, rhs_parameter)) {
            continue;
          }
          bool every_call_is_distinct =
              std::all_of(call_iter->second.begin(), call_iter->second.end(), [&](const DirectCall &call) {
                if (lhs_index >= call.arguments.size() || rhs_index >= call.arguments.size()) {
                  return false;
                }
                auto lhs_object = GetBaseObject(call.arguments[lhs_index]);
                auto rhs_object = GetBaseObject(call.arguments[rhs_index]);
                if (DefinitelyDistinct(lhs_object, rhs_object)) {
                  return true;
                }
                return lhs_object.kind == ObjectKind::Param && rhs_object.kind == ObjectKind::Param &&
                       AreProvenNoAliasParameters(lhs_object.root, rhs_object.root);
              });
          if (every_call_is_distinct) {
            noalias_parameters_[lhs_parameter].insert(rhs_parameter);
            noalias_parameters_[rhs_parameter].insert(lhs_parameter);
            noalias_changed = true;
          }
        }
      }
    }
  }

  // Seed memory summaries with direct stores. Writes to local allocations are
  // invisible to the caller; a parameter write is recorded at its argument
  // index, while globals and untrackable pointers remain conservative.
  for (const auto &[name, function] : functions) {
    auto parameters = function->GetArguments();
    FunctionMemorySummary summary;
    summary.parameter_may_write.resize(parameters.size(), false);
    std::unordered_map<mlir::Value *, size_t> parameter_index;
    for (size_t index = 0; index < parameters.size(); ++index) {
      parameter_index[parameters[index]] = index;
    }
    for (auto *block : function->GetBlocks()) {
      for (auto *operation : block->operations_) {
        auto *store = mlir::DynamicOrNull<mlir::var::VarSetOp>(operation);
        if (store == nullptr) {
          continue;
        }
        auto object = GetBaseObject(store->GetPtr());
        if (object.kind == ObjectKind::Local) {
          continue;
        }
        auto param_iter = parameter_index.find(object.root);
        if (object.kind == ObjectKind::Param && param_iter != parameter_index.end()) {
          summary.parameter_may_write[param_iter->second] = true;
        } else {
          summary.unknown_may_write = true;
        }
      }
    }
    function_memory_summaries_[name] = std::move(summary);
  }

  // Propagate callee effects to callers to a fixed point. External calls are
  // conservatively treated as arbitrary writes. For an internal call, writes
  // through a caller-local allocation do not escape into the caller summary.
  bool summary_changed = true;
  while (summary_changed) {
    summary_changed = false;
    for (const auto &[caller_name, function] : functions) {
      auto &caller_summary = function_memory_summaries_[caller_name];
      auto caller_parameters = function->GetArguments();
      std::unordered_map<mlir::Value *, size_t> caller_parameter_index;
      for (size_t index = 0; index < caller_parameters.size(); ++index) {
        caller_parameter_index[caller_parameters[index]] = index;
      }
      for (const auto &call : calls_by_caller[caller_name]) {
        auto callee_iter = function_memory_summaries_.find(call.name);
        if (callee_iter == function_memory_summaries_.end()) {
          if (!caller_summary.unknown_may_write) {
            caller_summary.unknown_may_write = true;
            summary_changed = true;
          }
          continue;
        }
        const auto &callee_summary = callee_iter->second;
        if (callee_summary.unknown_may_write && !caller_summary.unknown_may_write) {
          caller_summary.unknown_may_write = true;
          summary_changed = true;
        }
        for (size_t index = 0; index < callee_summary.parameter_may_write.size(); ++index) {
          if (!callee_summary.parameter_may_write[index]) {
            continue;
          }
          if (index >= call.arguments.size()) {
            if (!caller_summary.unknown_may_write) {
              caller_summary.unknown_may_write = true;
              summary_changed = true;
            }
            continue;
          }
          auto object = GetBaseObject(call.arguments[index]);
          if (object.kind == ObjectKind::Local) {
            continue;
          }
          auto param_iter = caller_parameter_index.find(object.root);
          if (object.kind == ObjectKind::Param && param_iter != caller_parameter_index.end()) {
            if (!caller_summary.parameter_may_write[param_iter->second]) {
              caller_summary.parameter_may_write[param_iter->second] = true;
              summary_changed = true;
            }
          } else if (!caller_summary.unknown_may_write) {
            caller_summary.unknown_may_write = true;
            summary_changed = true;
          }
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
    if (auto *phi_op = mlir::DynamicOrNull<mlir::base::PhiOp>(op)) {
      // A phi resolves to a definite object only if every incoming value derives from the same
      // one. A loop-carried address phi (e.g. an induction of a[i][j]'s address) references itself
      // through a getptr chain on the back-edge; those self-referential operands carry no new
      // information, so skip them and require the remaining operands to agree. This keeps a store
      // through such a phi anchored to its real array, unblocking invariant-load hoisting.
      result = ResolvePhiBaseObject(phi_op, current);
      break;
    }
    result = {ObjectKind::Unknown, current};
    break;
  }
  base_cache_[pointer] = result;
  return result;
}

auto AliasAnalysisPass::ResolvePhiBaseObject(mlir::base::PhiOp *phi, mlir::Value *phi_value) -> MemoryObject {
  // Guard against the cycle a loop-carried phi forms with itself: mark the phi's result as
  // in-progress so a recursive query returning to it is treated as "no new information" rather
  // than recursing forever.
  if (!phi_resolution_stack_.insert(phi_value).second) {
    return {ObjectKind::Unknown, phi_value};
  }

  bool has_definite = false;
  MemoryObject agreed;
  for (auto *incoming : phi->GetIncomingValue()) {
    // Operands that cycle back to this phi (directly, or through a getptr chain on the back-edge)
    // carry no new information about the object; skip them and let the entry operands decide.
    if (DerivesFromPhi(incoming, phi_value)) {
      continue;
    }
    auto object = GetBaseObject(incoming);
    if (!has_definite) {
      agreed = object;
      has_definite = true;
    } else if (agreed.kind != object.kind || agreed.root != object.root) {
      agreed = {ObjectKind::Unknown, nullptr};
      break;
    }
  }

  phi_resolution_stack_.erase(phi_value);
  if (!has_definite || agreed.kind == ObjectKind::Unknown) {
    return {ObjectKind::Unknown, phi_value};
  }
  return agreed;
}

auto AliasAnalysisPass::DerivesFromPhi(mlir::Value *value, mlir::Value *phi_value) -> bool {
  // Walk the getptr/copy chain feeding `value`; report whether it originates at `phi_value`.
  mlir::Value *current = value;
  while (current != nullptr && current->op_ != nullptr) {
    if (current == phi_value) {
      return true;
    }
    auto *op = current->op_;
    if (auto *getptr_op = mlir::DynamicOrNull<mlir::var::VarGetPtrOp>(op)) {
      current = getptr_op->GetPtr();
      continue;
    }
    if (auto *copy_op = mlir::DynamicOrNull<mlir::base::CopyOp>(op)) {
      current = copy_op->GetSrc();
      continue;
    }
    break;
  }
  return current == phi_value;
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
  if (lhs_object.kind == ObjectKind::Param && rhs_object.kind == ObjectKind::Param &&
      AreProvenNoAliasParameters(lhs_object.root, rhs_object.root)) {
    return AliasResult::NoAlias;
  }
  if (DefinitelyDistinct(lhs_object, rhs_object)) {
    return AliasResult::NoAlias;
  }
  return AliasResult::MayAlias;
}

auto AliasAnalysisPass::AreProvenNoAliasParameters(mlir::Value *lhs, mlir::Value *rhs) const -> bool {
  auto iter = noalias_parameters_.find(lhs);
  return iter != noalias_parameters_.end() && iter->second.count(rhs) > 0;
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
  auto object = GetBaseObject(pointer);
  // A call may touch everything except a local whose address never escapes the function.
  bool is_non_escaped_local = object.kind == ObjectKind::Local && !IsEscaped(object);
  if (is_non_escaped_local) {
    return false;
  }

  DirectCall direct_call;
  if (!GetDirectCall(call, &direct_call)) {
    return true;
  }
  auto summary_iter = function_memory_summaries_.find(direct_call.name);
  if (summary_iter == function_memory_summaries_.end() || summary_iter->second.unknown_may_write) {
    return true;
  }
  const auto &summary = summary_iter->second;
  for (size_t index = 0; index < summary.parameter_may_write.size(); ++index) {
    if (!summary.parameter_may_write[index]) {
      continue;
    }
    if (index >= direct_call.arguments.size() || Alias(pointer, direct_call.arguments[index]) != AliasResult::NoAlias) {
      return true;
    }
  }
  return false;
}

auto AliasAnalysisPass::IsEscaped(const MemoryObject &object) -> bool {
  return object.kind == ObjectKind::Local && escaped_locals_.count(object.root) > 0;
}

}  // namespace middleend::pass::analysis
