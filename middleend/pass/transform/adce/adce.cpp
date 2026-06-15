#include "middleend/pass/transform/adce/adce.h"
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/arithm/basic.h"
#include "middleend/mlir/dialect/base/basic.h"
#include "middleend/mlir/dialect/base/branch.h"
#include "middleend/mlir/dialect/base/copy.h"
#include "middleend/mlir/dialect/base/funccall.h"
#include "middleend/mlir/dialect/base/funcdef.h"
#include "middleend/mlir/dialect/base/phi.h"
#include "middleend/mlir/dialect/func/basic.h"
#include "middleend/mlir/dialect/func/funccall.h"
#include "middleend/mlir/dialect/func/return.h"
#include "middleend/mlir/dialect/var/basic.h"
#include "middleend/mlir/dialect/var/set.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/block.h"
#include "middleend/pass/analysis/cfg/cfg.h"
#include "middleend/pass/analysis/cfg/mlircfg.h"
#include "middleend/pass/analysis/domtree/domtree.h"

#define GET_MLIR_CONTEXT() compiler_context_->GetIRContext()->GetMLIRContext()
#define GET_PASS_CONTEXT() compiler_context_->GetPassContext()

namespace middleend::pass::transform {
auto ADCEPass::Run() -> void {
  PreRun();
  auto *cfg_pass = GET_PASS_CONTEXT()->GetSpecPass<analysis::CFGPass>("CFGPass");
  auto *dom_tree_pass = GET_PASS_CONTEXT()->GetSpecPass<analysis::DomTreePass>("DomTreePass");
  if (cfg_pass == nullptr || dom_tree_pass == nullptr) {
    throw std::logic_error("ADCEPass requires CFGPass and DomTreePass");
  }

  bool changed = false;
  for (auto *function_cfg : cfg_pass->Get()) {
    auto *mlir_function_cfg = dynamic_cast<analysis::MLIRFunctionCFG *>(function_cfg);
    if (mlir_function_cfg == nullptr) {
      throw std::logic_error("ADCEPass only supports MLIRFunctionCFG");
    }
    auto *post_dom_tree = dom_tree_pass->GetPostDomTree(function_cfg);
    if (post_dom_tree == nullptr) {
      compiler_context_->Print("ADCEPass: skip function without post-dominator tree\n");
      continue;
    }
    changed = RunOnFunction(mlir_function_cfg, post_dom_tree) || changed;
  }

  if (changed) {
    GET_PASS_CONTEXT()->InvalidateAnalysis();
  }
}

auto ADCEPass::RunOnFunction(analysis::MLIRFunctionCFG *function_cfg, analysis::DomTree *post_dom_tree) -> bool {
  ResetFunctionState();
  current_function_cfg_ = function_cfg;
  current_post_dom_tree_ = post_dom_tree;

  for (auto *cfg_node : current_function_cfg_->GetNodes()) {
    auto *mlir_cfg_node = dynamic_cast<analysis::MLIRCFGNode *>(cfg_node);
    if (mlir_cfg_node == nullptr) {
      throw std::logic_error("ADCEPass only supports MLIRCFGNode");
    }
    auto *block = mlir_cfg_node->GetBlock();
    for (auto *operation : block->operations_) {
      operation_to_block_[operation] = block;
    }
  }

  CollectRootOperations();
  PropagateLiveness();

  bool changed = RewriteDeadTerminators();
  PropagateLiveness();
  changed = RemoveDeadOperations() || changed;

  ResetFunctionState();
  return changed;
}

auto ADCEPass::CollectRootOperations() -> void {
  for (auto *cfg_node : current_function_cfg_->GetNodes()) {
    auto *mlir_cfg_node = dynamic_cast<analysis::MLIRCFGNode *>(cfg_node);
    if (mlir_cfg_node == nullptr) {
      throw std::logic_error("ADCEPass only supports MLIRCFGNode");
    }
    for (auto *operation : mlir_cfg_node->GetBlock()->operations_) {
      if (IsAlwaysLive(operation)) {
        MarkOperationLive(operation);
      }
    }
  }
}

auto ADCEPass::PropagateLiveness() -> void {
  while (!live_worklist_.empty() || !new_live_blocks_.empty()) {
    while (!live_worklist_.empty()) {
      auto *operation = live_worklist_.front();
      live_worklist_.pop_front();
      for (auto *operand : operation->operands_) {
        if (operand != nullptr && operand->op_ != nullptr) {
          MarkOperationLive(operand->op_);
        }
      }
      MarkPhiPredecessorsLive(operation);
    }
    MarkControlDependentBranches();
  }
}

auto ADCEPass::MarkOperationLive(mlir::Operation *operation) -> void {
  if (operation == nullptr || live_operations_.count(operation) > 0) {
    return;
  }

  auto *block = GetBlockOfOperation(operation);
  if (block == nullptr) {
    // The defining op is outside the current function CFG, e.g. a global definition.
    return;
  }
  auto *cfg_node = current_function_cfg_->GetNodeByBlock(block);
  if (cfg_node == nullptr) {
    // The defining op is not part of the reachable CFG built for this function.
    return;
  }

  live_operations_.insert(operation);
  live_worklist_.emplace_back(operation);
  MarkBlockLive(cfg_node);

  if (auto *branch_op = mlir::DynamicOrNull<mlir::base::BrOp>(operation)) {
    auto *target_node = current_function_cfg_->GetNodeByBlock(branch_op->GetTarget());
    if (target_node != nullptr) {
      MarkBlockLive(target_node);
    }
  }
}

auto ADCEPass::MarkBlockLive(analysis::CFGNode *cfg_node) -> void {
  if (cfg_node == nullptr || live_blocks_.count(cfg_node) > 0) {
    return;
  }
  live_blocks_.insert(cfg_node);
  new_live_blocks_.emplace_back(cfg_node);

  auto *mlir_cfg_node = dynamic_cast<analysis::MLIRCFGNode *>(cfg_node);
  if (mlir_cfg_node == nullptr) {
    throw std::logic_error("ADCEPass only supports MLIRCFGNode");
  }
  auto *terminator = GetTerminator(mlir_cfg_node->GetBlock());
  if (mlir::DynamicOrNull<mlir::base::BrOp>(terminator) != nullptr) {
    MarkOperationLive(terminator);
  }
}

auto ADCEPass::MarkPhiPredecessorsLive(mlir::Operation *operation) -> void {
  auto *phi_op = mlir::DynamicOrNull<mlir::base::PhiOp>(operation);
  if (phi_op == nullptr) {
    return;
  }
  for (auto *incoming_block : phi_op->GetIncomingBlock()) {
    auto *predecessor_node = current_function_cfg_->GetNodeByBlock(incoming_block);
    if (predecessor_node != nullptr) {
      MarkBlockLive(predecessor_node);
    }
  }
}

auto ADCEPass::MarkControlDependentBranches() -> void {
  while (!new_live_blocks_.empty()) {
    auto *live_node = new_live_blocks_.front();
    new_live_blocks_.pop_front();
    for (auto *cfg_node : current_function_cfg_->GetNodes()) {
      auto *mlir_cfg_node = dynamic_cast<analysis::MLIRCFGNode *>(cfg_node);
      if (mlir_cfg_node == nullptr) {
        throw std::logic_error("ADCEPass only supports MLIRCFGNode");
      }
      auto *terminator = GetTerminator(mlir_cfg_node->GetBlock());
      if (mlir::DynamicOrNull<mlir::base::CondBrOp>(terminator) == nullptr) {
        continue;
      }
      if (live_operations_.count(terminator) > 0) {
        continue;
      }
      if (IsControlDependent(cfg_node, live_node)) {
        MarkOperationLive(terminator);
      }
    }
  }
}

auto ADCEPass::IsControlDependent(analysis::CFGNode *branch_node, analysis::CFGNode *live_node) -> bool {
  auto *branch_dom_node = current_post_dom_tree_->GetDomTreeNode(branch_node);
  if (branch_dom_node == nullptr) {
    return false;
  }
  auto *ipdom = branch_dom_node->GetIDom();
  for (auto *successor : branch_node->GetSuccessors()) {
    auto *cursor = current_post_dom_tree_->GetDomTreeNode(successor);
    while (cursor != nullptr && cursor != ipdom) {
      if (cursor->GetCFGNode() == live_node) {
        return true;
      }
      cursor = cursor->GetIDom();
    }
  }
  return false;
}

auto ADCEPass::RewriteDeadTerminators() -> bool {
  bool changed = false;
  for (auto *cfg_node : current_function_cfg_->GetNodes()) {
    auto *mlir_cfg_node = dynamic_cast<analysis::MLIRCFGNode *>(cfg_node);
    if (mlir_cfg_node == nullptr) {
      throw std::logic_error("ADCEPass only supports MLIRCFGNode");
    }
    auto *block = mlir_cfg_node->GetBlock();
    auto *terminator = GetTerminator(block);
    if (terminator == nullptr || live_operations_.count(terminator) > 0) {
      continue;
    }
    if (mlir::DynamicOrNull<mlir::base::CondBrOp>(terminator) != nullptr) {
      bool has_live_successor = false;
      for (auto *successor : cfg_node->GetSuccessors()) {
        if (live_blocks_.count(successor) > 0) {
          has_live_successor = true;
          break;
        }
      }
      if (has_live_successor) {
        MarkOperationLive(terminator);
        continue;
      }
    }
    auto *target_block = FindNearestLivePostDominator(cfg_node);
    if (target_block == nullptr) {
      throw std::logic_error("ADCEPass failed to find live post-dominator target");
    }
    RemoveOperandUses(terminator);
    auto *branch_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::BrOp>(target_block);
    block->operations_.back() = branch_op;
    operation_to_block_[branch_op] = block;
    changed = true;
  }
  return changed;
}

auto ADCEPass::RemoveDeadOperations() -> bool {
  bool changed = false;
  for (auto *cfg_node : current_function_cfg_->GetNodes()) {
    auto *mlir_cfg_node = dynamic_cast<analysis::MLIRCFGNode *>(cfg_node);
    if (mlir_cfg_node == nullptr) {
      throw std::logic_error("ADCEPass only supports MLIRCFGNode");
    }
    auto *block = mlir_cfg_node->GetBlock();
    std::vector<mlir::Operation *> new_operations;
    new_operations.reserve(block->operations_.size());
    for (auto *operation : block->operations_) {
      if (IsTerminator(operation) || live_operations_.count(operation) > 0) {
        new_operations.emplace_back(operation);
        continue;
      }
      RemoveOperandUses(operation);
      for (auto *result : operation->results_) {
        if (result != nullptr) {
          result->ClearUses();
          GET_PASS_CONTEXT()->InsertDeletedValue(result);
        }
      }
      changed = true;
    }
    block->operations_ = std::move(new_operations);
  }
  return changed;
}

auto ADCEPass::GetTerminator(mlir::Block *block) -> mlir::Operation * {
  if (block == nullptr || block->operations_.empty()) {
    return nullptr;
  }
  return block->operations_.back();
}

auto ADCEPass::GetBlockOfOperation(mlir::Operation *operation) -> mlir::Block * {
  if (operation_to_block_.count(operation) == 0) {
    return nullptr;
  }
  return operation_to_block_[operation];
}

auto ADCEPass::FindNearestLivePostDominator(analysis::CFGNode *cfg_node) -> mlir::Block * {
  auto *dom_tree_node = current_post_dom_tree_->GetDomTreeNode(cfg_node);
  if (dom_tree_node == nullptr) {
    return nullptr;
  }
  for (auto *cursor = dom_tree_node->GetIDom(); cursor != nullptr; cursor = cursor->GetIDom()) {
    if (live_blocks_.count(cursor->GetCFGNode()) == 0) {
      continue;
    }
    auto *target_node = dynamic_cast<analysis::MLIRCFGNode *>(cursor->GetCFGNode());
    if (target_node == nullptr) {
      throw std::logic_error("ADCEPass only supports MLIRCFGNode");
    }
    return target_node->GetBlock();
  }
  return nullptr;
}

auto ADCEPass::IsAlwaysLive(mlir::Operation *operation) -> bool {
  if (mlir::DynamicOrNull<mlir::func::ReturnOp>(operation) != nullptr) {
    return true;
  }
  if (mlir::DynamicOrNull<mlir::var::VarSetOp>(operation) != nullptr) {
    return true;
  }
  if (mlir::DynamicOrNull<mlir::base::FuncCallOp>(operation) != nullptr ||
      mlir::DynamicOrNull<mlir::func::FuncCallOp>(operation) != nullptr) {
    return true;
  }
  if (mlir::DynamicOrNull<mlir::base::BrOp>(operation) != nullptr ||
      mlir::DynamicOrNull<mlir::base::CondBrOp>(operation) != nullptr) {
    return false;
  }
  if (dynamic_cast<mlir::arithm::ArithmOps *>(operation) != nullptr) {
    return false;
  }
  if (mlir::DynamicOrNull<mlir::base::PhiOp>(operation) != nullptr ||
      mlir::DynamicOrNull<mlir::base::CopyOp>(operation) != nullptr) {
    return false;
  }
  if (dynamic_cast<mlir::base::BaseOps *>(operation) != nullptr) {
    return false;
  }
  if (dynamic_cast<mlir::var::VarOps *>(operation) != nullptr) {
    return false;
  }
  if (dynamic_cast<mlir::func::FuncOps *>(operation) != nullptr) {
    return true;
  }
  return true;
}

auto ADCEPass::IsTerminator(mlir::Operation *operation) -> bool {
  return mlir::DynamicOrNull<mlir::base::BrOp>(operation) != nullptr ||
         mlir::DynamicOrNull<mlir::base::CondBrOp>(operation) != nullptr ||
         mlir::DynamicOrNull<mlir::func::ReturnOp>(operation) != nullptr;
}

auto ADCEPass::RemoveOperandUses(mlir::Operation *operation) -> void {
  for (auto *operand : operation->operands_) {
    if (operand != nullptr) {
      operand->RemoveUse(operation);
    }
  }
}

auto ADCEPass::ResetFunctionState() -> void {
  current_function_cfg_ = nullptr;
  current_post_dom_tree_ = nullptr;
  operation_to_block_.clear();
  live_operations_.clear();
  live_blocks_.clear();
  live_worklist_.clear();
  new_live_blocks_.clear();
}
}  // namespace middleend::pass::transform
