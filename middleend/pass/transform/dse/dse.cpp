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
  alias_analysis_ = GET_PASS_CONTEXT()->GetSpecPass<analysis::AliasAnalysisPass>("AliasAnalysisPass");
  if (cfg_pass == nullptr || alias_analysis_ == nullptr) {
    throw std::logic_error("DSEPass requires CFGPass and AliasAnalysisPass");
  }

  ComputeLoadedLocals();

  changed_ = false;
  for (auto *function_cfg : cfg_pass->Get()) {
    auto *mlir_function_cfg = dynamic_cast<analysis::MLIRFunctionCFG *>(function_cfg);
    if (mlir_function_cfg == nullptr) {
      throw std::logic_error("DSEPass only supports MLIRFunctionCFG");
    }
    for (auto *cfg_node : mlir_function_cfg->GetNodes()) {
      auto *mlir_node = dynamic_cast<analysis::MLIRCFGNode *>(cfg_node);
      if (mlir_node != nullptr) {
        changed_ = RunOnBlock(mlir_node->GetBlock()) || changed_;
      }
    }
  }

  if (changed_) {
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

auto DSEPass::RunOnBlock(mlir::Block *block) -> bool {
  std::unordered_set<mlir::Operation *> dead;
  // Stores whose value has not yet been observed nor overwritten, in program order.
  std::vector<std::pair<mlir::Operation *, mlir::Value *>> pending;

  for (auto *operation : block->operations_) {
    if (auto *store_op = mlir::DynamicOrNull<mlir::var::VarSetOp>(operation)) {
      auto *pointer = store_op->GetPtr();
      if (IsWriteOnlyDead(pointer)) {
        dead.insert(operation);
        continue;
      }
      // A must-alias overwrite kills any still-pending store to the same location.
      std::vector<std::pair<mlir::Operation *, mlir::Value *>> survivors;
      for (const auto &entry : pending) {
        if (alias_analysis_->Alias(pointer, entry.second) == analysis::AliasResult::MustAlias) {
          dead.insert(entry.first);
        } else {
          survivors.emplace_back(entry);
        }
      }
      survivors.emplace_back(operation, pointer);
      pending = std::move(survivors);
      continue;
    }

    if (auto *load_op = mlir::DynamicOrNull<mlir::var::VarGetValOp>(operation)) {
      // A may-alias load observes the stored value, ending the dead window.
      auto *pointer = load_op->GetPtr();
      std::vector<std::pair<mlir::Operation *, mlir::Value *>> survivors;
      for (const auto &entry : pending) {
        if (alias_analysis_->Alias(pointer, entry.second) == analysis::AliasResult::NoAlias) {
          survivors.emplace_back(entry);
        }
      }
      pending = std::move(survivors);
      continue;
    }

    if (IsCall(operation)) {
      // A call may read the stored value, ending the dead window.
      std::vector<std::pair<mlir::Operation *, mlir::Value *>> survivors;
      for (const auto &entry : pending) {
        if (!alias_analysis_->CallModRef(operation, entry.second)) {
          survivors.emplace_back(entry);
        }
      }
      pending = std::move(survivors);
      continue;
    }
  }

  if (dead.empty()) {
    return false;
  }
  RemoveStores(block, dead);
  return true;
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
