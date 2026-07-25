#include "middleend/pass/transform/dse/dse.h"
#include <stdexcept>
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/base/funccall.h"
#include "middleend/mlir/dialect/func/funccall.h"
#include "middleend/mlir/dialect/var/getvalue.h"
#include "middleend/mlir/dialect/var/set.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/block.h"
#include "middleend/mlir/utils/value/value.h"
#include "middleend/pass/analysis/alias/alias.h"
#include "middleend/pass/analysis/cfg/cfg.h"
#include "middleend/pass/analysis/cfg/mlircfg.h"

#define GET_PASS_CONTEXT() compiler_context_->GetPassContext()

namespace middleend::pass::transform {
namespace {
auto IsCall(mlir::Operation *operation) -> bool {
  return mlir::DynamicOrNull<mlir::base::FuncCallOp>(operation) != nullptr ||
         mlir::DynamicOrNull<mlir::func::FuncCallOp>(operation) != nullptr;
}
}  // namespace

auto DSEPass::Run() -> void {
  PreRun();
  auto *cfg_pass = GET_PASS_CONTEXT()->GetSpecPass<analysis::CFGPass>("CFGPass");
  const char *alias_pass_name = before_promotion_ ? "PrePromotionAliasAnalysisPass" : "AliasAnalysisPass";
  alias_analysis_ = GET_PASS_CONTEXT()->GetSpecPass<analysis::AliasAnalysisPass>(alias_pass_name);
  if (cfg_pass == nullptr || alias_analysis_ == nullptr) {
    throw std::logic_error("DSEPass requires CFGPass and AliasAnalysisPass");
  }

  ComputeLoadedLocals();

  bool changed = false;
  for (auto *function_cfg : cfg_pass->Get()) {
    auto *mlir_function_cfg = dynamic_cast<analysis::MLIRFunctionCFG *>(function_cfg);
    if (mlir_function_cfg == nullptr) {
      throw std::logic_error("DSEPass only supports MLIRFunctionCFG");
    }
    changed = RunOnFunction(mlir_function_cfg) || changed;
  }

  if (changed) {
    GET_PASS_CONTEXT()->InvalidateAnalysis();
  }
}

auto DSEPass::ComputeLoadedLocals() -> void {
  loaded_local_roots_.clear();
  auto *cfg_pass = GET_PASS_CONTEXT()->GetSpecPass<analysis::CFGPass>("CFGPass");
  for (auto *function_cfg : cfg_pass->Get()) {
    auto *mlir_function_cfg = dynamic_cast<analysis::MLIRFunctionCFG *>(function_cfg);
    if (mlir_function_cfg == nullptr) {
      continue;
    }
    for (auto *cfg_node : mlir_function_cfg->GetNodes()) {
      auto *mlir_node = dynamic_cast<analysis::MLIRCFGNode *>(cfg_node);
      if (mlir_node == nullptr) {
        continue;
      }
      for (auto *operation : mlir_node->GetBlock()->operations_) {
        if (auto *load_op = mlir::DynamicOrNull<mlir::var::VarGetValOp>(operation)) {
          auto object = alias_analysis_->GetBaseObject(load_op->GetPtr());
          if (object.kind == analysis::AliasAnalysisPass::ObjectKind::Local) {
            loaded_local_roots_.insert(object.root);
          }
        }
      }
    }
  }
}

auto DSEPass::IsWriteOnlyDead(mlir::Value *pointer) -> bool {
  auto object = alias_analysis_->GetBaseObject(pointer);
  return object.kind == analysis::AliasAnalysisPass::ObjectKind::Local && !alias_analysis_->IsEscaped(object) &&
         loaded_local_roots_.count(object.root) == 0;
}

auto DSEPass::RunOnFunction(analysis::MLIRFunctionCFG *function_cfg) -> bool {
  current_cfg_ = function_cfg;
  std::unordered_map<mlir::Block *, std::unordered_set<mlir::Operation *>> dead;

  for (auto *cfg_node : function_cfg->GetNodes()) {
    auto *mlir_node = dynamic_cast<analysis::MLIRCFGNode *>(cfg_node);
    if (mlir_node == nullptr) {
      continue;
    }
    auto *block = mlir_node->GetBlock();
    const auto &operations = block->operations_;
    for (size_t index = 0; index < operations.size(); ++index) {
      auto *store_op = mlir::DynamicOrNull<mlir::var::VarSetOp>(operations[index]);
      if (store_op == nullptr) {
        continue;
      }
      auto *pointer = store_op->GetPtr();
      if (IsWriteOnlyDead(pointer) || IsDeadByOverwrite(block, index, pointer)) {
        dead[block].insert(operations[index]);
      }
    }
  }

  bool changed = false;
  for (auto &entry : dead) {
    RemoveStores(entry.first, entry.second);
    changed = true;
  }
  current_cfg_ = nullptr;
  return changed;
}

auto DSEPass::IsDeadByOverwrite(mlir::Block *block, size_t store_index, mlir::Value *pointer) -> bool {
  std::unordered_set<mlir::Block *> on_stack;
  std::unordered_map<mlir::Block *, bool> memo;
  on_stack.insert(block);
  // Start scanning right after the candidate store; its own block is on the stack so a
  // back-edge returning to it is treated conservatively as "not overwritten".
  return TailOverwritten(block, store_index + 1, pointer, &on_stack, &memo);
}

auto DSEPass::TailOverwritten(mlir::Block *block, size_t begin, mlir::Value *pointer,
                              std::unordered_set<mlir::Block *> *on_stack,
                              std::unordered_map<mlir::Block *, bool> *memo) -> bool {
  const auto &operations = block->operations_;
  for (size_t index = begin; index < operations.size(); ++index) {
    auto *operation = operations[index];
    if (auto *store_op = mlir::DynamicOrNull<mlir::var::VarSetOp>(operation)) {
      // A must-alias store fully overwrites the location: this path is covered.
      if (alias_analysis_->Alias(pointer, store_op->GetPtr()) == analysis::AliasResult::MustAlias) {
        return true;
      }
      // A may-alias / no-alias store neither reads nor guarantees overwrite: keep scanning.
      continue;
    }
    if (auto *load_op = mlir::DynamicOrNull<mlir::var::VarGetValOp>(operation)) {
      // A may-alias load observes the stored value: the store is live on this path.
      if (alias_analysis_->Alias(pointer, load_op->GetPtr()) != analysis::AliasResult::NoAlias) {
        return false;
      }
      continue;
    }
    if (IsCall(operation)) {
      // A call that may reference the location observes the stored value.
      if (alias_analysis_->CallModRefPrecise(operation, pointer)) {
        return false;
      }
      continue;
    }
  }

  // Fell off the end of the block: every successor path must also be overwritten.
  auto *node = current_cfg_->GetNodeByBlock(block);
  if (node == nullptr) {
    return false;
  }
  auto successors = node->GetSuccessors();
  if (successors.empty()) {
    return false;  // reached a function exit without overwriting.
  }
  for (auto *successor : successors) {
    auto *successor_node = dynamic_cast<analysis::MLIRCFGNode *>(successor);
    if (successor_node == nullptr) {
      return false;
    }
    if (!BlockEntryOverwritten(successor_node->GetBlock(), pointer, on_stack, memo)) {
      return false;
    }
  }
  return true;
}

auto DSEPass::BlockEntryOverwritten(mlir::Block *block, mlir::Value *pointer,
                                    std::unordered_set<mlir::Block *> *on_stack,
                                    std::unordered_map<mlir::Block *, bool> *memo) -> bool {
  auto cached = memo->find(block);
  if (cached != memo->end()) {
    return cached->second;
  }
  if (on_stack->count(block) > 0) {
    // Back-edge: a loop path that has not yet overwritten the location. Stay conservative;
    // a "true" result is never produced through a back-edge, so this cannot delete a live store.
    return false;
  }
  on_stack->insert(block);
  bool result = TailOverwritten(block, 0, pointer, on_stack, memo);
  on_stack->erase(block);
  (*memo)[block] = result;
  return result;
}

auto DSEPass::RemoveStores(mlir::Block *block, const std::unordered_set<mlir::Operation *> &dead) -> void {
  std::vector<mlir::Operation *> kept;
  kept.reserve(block->operations_.size());
  for (auto *operation : block->operations_) {
    if (dead.count(operation) == 0) {
      kept.emplace_back(operation);
      continue;
    }
    for (auto *operand : operation->operands_) {
      if (operand != nullptr) {
        operand->RemoveUse(operation);
      }
    }
  }
  block->operations_ = std::move(kept);
}

}  // namespace middleend::pass::transform
