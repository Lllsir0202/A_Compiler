#include "middleend/pass/transform/promotion/promotion.h"
#include <algorithm>
#include <stdexcept>
#include <vector>
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/base/branch.h"
#include "middleend/mlir/dialect/base/funccall.h"
#include "middleend/mlir/dialect/base/phi.h"
#include "middleend/mlir/dialect/func/funccall.h"
#include "middleend/mlir/dialect/var/getvalue.h"
#include "middleend/mlir/dialect/var/set.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/block.h"
#include "middleend/mlir/utils/value/value.h"
#include "middleend/pass/analysis/alias/alias.h"
#include "middleend/pass/analysis/cfg/cfg.h"
#include "middleend/pass/analysis/cfg/mlircfg.h"
#include "middleend/pass/analysis/domtree/domtree.h"
#include "middleend/pass/analysis/loopinfo/loopinfo.h"

#define GET_MLIR_CONTEXT() compiler_context_->GetIRContext()->GetMLIRContext()
#define GET_PASS_CONTEXT() compiler_context_->GetPassContext()

namespace middleend::pass::transform {
namespace {
template <typename OperationType>
auto Is(mlir::Operation *operation) -> bool {
  return mlir::DynamicOrNull<OperationType>(operation) != nullptr;
}
auto IsStore(mlir::Operation *operation) -> bool { return Is<mlir::var::VarSetOp>(operation); }
auto IsLoad(mlir::Operation *operation) -> bool { return Is<mlir::var::VarGetValOp>(operation); }
auto IsCall(mlir::Operation *operation) -> bool {
  return Is<mlir::base::FuncCallOp>(operation) || Is<mlir::func::FuncCallOp>(operation);
}

// Index of an operation inside its block, for ordering two ops in the same block.
auto IndexInBlock(mlir::Block *block, mlir::Operation *operation) -> int {
  for (size_t i = 0; i < block->operations_.size(); ++i) {
    if (block->operations_[i] == operation) {
      return static_cast<int>(i);
    }
  }
  return -1;
}
}  // namespace

auto PromotionPass::Run() -> void {
  PreRun();
  auto *cfg_pass = GET_PASS_CONTEXT()->GetSpecPass<analysis::CFGPass>("CFGPass");
  auto *dom_tree_pass = GET_PASS_CONTEXT()->GetSpecPass<analysis::DomTreePass>("DomTreePass");
  auto *loop_info = GET_PASS_CONTEXT()->GetSpecPass<analysis::LoopInfoPass>("LoopInfoPass");
  alias_analysis_ = GET_PASS_CONTEXT()->GetSpecPass<analysis::AliasAnalysisPass>("AliasAnalysisPass");
  if (cfg_pass == nullptr || dom_tree_pass == nullptr || loop_info == nullptr || alias_analysis_ == nullptr) {
    throw std::logic_error("PromotionPass requires CFGPass, DomTreePass, LoopInfoPass and AliasAnalysisPass");
  }

  bool changed = false;
  for (auto *function_cfg : cfg_pass->Get()) {
    auto *mlir_function_cfg = dynamic_cast<analysis::MLIRFunctionCFG *>(function_cfg);
    if (mlir_function_cfg == nullptr) {
      throw std::logic_error("PromotionPass only supports MLIRFunctionCFG");
    }
    auto *dom_tree = dom_tree_pass->GetDomTree(function_cfg);
    if (dom_tree == nullptr) {
      continue;
    }
    current_cfg_ = mlir_function_cfg;
    op_to_block_.clear();
    for (auto *cfg_node : mlir_function_cfg->GetNodes()) {
      auto *mlir_node = dynamic_cast<analysis::MLIRCFGNode *>(cfg_node);
      if (mlir_node == nullptr) {
        continue;
      }
      for (auto *operation : mlir_node->GetBlock()->operations_) {
        op_to_block_[operation] = mlir_node->GetBlock();
      }
    }
    for (auto *loop : loop_info->GetLoops(function_cfg)) {
      changed = RunOnLoop(mlir_function_cfg, dom_tree, loop) || changed;
    }
    current_cfg_ = nullptr;
  }

  if (changed) {
    GET_PASS_CONTEXT()->InvalidateAnalysis();
  }
}

auto PromotionPass::RunOnLoop(analysis::MLIRFunctionCFG *function_cfg, analysis::DomTree *dom_tree,
                              analysis::Loop *loop) -> bool {
  auto *preheader = FindPreheader(function_cfg, loop);
  if (preheader == nullptr || preheader->operations_.empty()) {
    return false;
  }
  std::vector<mlir::Block *> exit_blocks;
  if (!CollectExits(loop, &exit_blocks) || exit_blocks.empty()) {
    return false;
  }

  // Validate every single-store location against the original loop, then apply. Promotable
  // locations are mutually no-alias, so applying one never invalidates another.
  std::vector<Candidate> candidates;
  for (auto *block : loop->GetBlocks()) {
    for (auto *operation : block->operations_) {
      if (!IsStore(operation)) {
        continue;
      }
      Candidate candidate;
      if (TryBuildCandidate(dom_tree, loop, operation, preheader, exit_blocks, &candidate)) {
        candidates.emplace_back(std::move(candidate));
      }
    }
  }

  for (const auto &candidate : candidates) {
    Promote(loop, candidate, preheader);
  }
  return !candidates.empty();
}

auto PromotionPass::FindPreheader(analysis::MLIRFunctionCFG *function_cfg, analysis::Loop *loop) -> mlir::Block * {
  auto *header_node = function_cfg->GetNodeByBlock(loop->GetHeader());
  if (header_node == nullptr) {
    return nullptr;
  }
  mlir::Block *preheader = nullptr;
  for (auto *predecessor : header_node->GetPredecessors()) {
    auto *mlir_pred = dynamic_cast<analysis::MLIRCFGNode *>(predecessor);
    if (mlir_pred == nullptr || loop->Contains(mlir_pred->GetBlock())) {
      continue;
    }
    if (preheader != nullptr) {
      return nullptr;
    }
    preheader = mlir_pred->GetBlock();
  }
  return preheader;
}

auto PromotionPass::CollectExits(analysis::Loop *loop, std::vector<mlir::Block *> *exit_blocks) -> bool {
  std::vector<mlir::Block *> exits;
  for (auto *block : loop->GetBlocks()) {
    auto *node = current_cfg_->GetNodeByBlock(block);
    if (node == nullptr) {
      return false;
    }
    for (auto *successor : node->GetSuccessors()) {
      auto *mlir_succ = dynamic_cast<analysis::MLIRCFGNode *>(successor);
      if (mlir_succ == nullptr || loop->Contains(mlir_succ->GetBlock())) {
        continue;
      }
      if (std::find(exits.begin(), exits.end(), mlir_succ->GetBlock()) == exits.end()) {
        exits.emplace_back(mlir_succ->GetBlock());
      }
    }
  }
  // Dedicated exits: every predecessor of an exit block must lie inside the loop, so a store
  // inserted at the exit runs only when leaving the loop.
  for (auto *exit : exits) {
    auto *node = current_cfg_->GetNodeByBlock(exit);
    if (node == nullptr) {
      return false;
    }
    for (auto *predecessor : node->GetPredecessors()) {
      auto *mlir_pred = dynamic_cast<analysis::MLIRCFGNode *>(predecessor);
      if (mlir_pred == nullptr || !loop->Contains(mlir_pred->GetBlock())) {
        return false;
      }
    }
  }
  *exit_blocks = std::move(exits);
  return true;
}

auto PromotionPass::TryBuildCandidate(analysis::DomTree *dom_tree, analysis::Loop *loop, mlir::Operation *store,
                                      mlir::Block *preheader, const std::vector<mlir::Block *> &exit_blocks,
                                      Candidate *candidate) -> bool {
  auto *store_op = mlir::DynamicOrNull<mlir::var::VarSetOp>(store);
  auto *pointer = store_op->GetPtr();
  auto *stored_value = store_op->GetNewValue();
  if (!IsInvariant(pointer, loop)) {
    return false;
  }
  // The preheader load we will insert reads `pointer`, so its definition must dominate the
  // preheader (guaranteed for a value used in the loop, but checked defensively).
  if (pointer->op_ != nullptr) {
    auto iter = op_to_block_.find(pointer->op_);
    if (iter != op_to_block_.end() && !Dominates(dom_tree, iter->second, preheader)) {
      return false;
    }
  }

  // Scan every memory operation in the loop: exactly one store may write this location, all
  // loads of it must be must-alias, and nothing else may alias it.
  std::vector<mlir::Operation *> loads;
  for (auto *block : loop->GetBlocks()) {
    for (auto *operation : block->operations_) {
      if (IsStore(operation)) {
        auto result = alias_analysis_->Alias(pointer, mlir::DynamicOrNull<mlir::var::VarSetOp>(operation)->GetPtr());
        if (operation == store) {
          continue;
        }
        if (result != analysis::AliasResult::NoAlias) {
          return false;  // a second must/may-alias store: not the single-store subset
        }
      } else if (IsLoad(operation)) {
        auto result = alias_analysis_->Alias(pointer, mlir::DynamicOrNull<mlir::var::VarGetValOp>(operation)->GetPtr());
        if (result == analysis::AliasResult::MustAlias) {
          loads.emplace_back(operation);
        } else if (result != analysis::AliasResult::NoAlias) {
          return false;  // a may-alias load may read a different-but-overlapping location
        }
      } else if (IsCall(operation) && alias_analysis_->CallModRef(operation, pointer)) {
        return false;
      }
    }
  }
  // The stored value must not itself be one of the promoted loads (a self-copy `p = p`).
  if (std::find(loads.begin(), loads.end(), stored_value->op_) != loads.end()) {
    return false;
  }

  // The store must run on every complete iteration (dominate every latch) so the value carried
  // across the back-edge is always the stored value -- one header phi then suffices.
  for (auto *latch : loop->GetLatches()) {
    if (!StoreDominatesBlockExit(dom_tree, store, latch)) {
      return false;
    }
  }

  candidate->store = store;
  candidate->pointer = pointer;
  candidate->stored_value = stored_value;
  candidate->loads = loads;
  candidate->needs_phi = false;

  // Each load reads the stored value if the store dominates it, else the carried (phi) value.
  for (auto *load : loads) {
    bool uses_stored = StoreDominatesOp(dom_tree, store, load);
    candidate->load_uses_stored.emplace_back(uses_stored);
    candidate->needs_phi = candidate->needs_phi || !uses_stored;
  }

  // For each exit, all its in-loop predecessors must agree on whether the store has run, so a
  // single value (stored or carried) reaches the inserted exit store without needing a phi there.
  for (auto *exit : exit_blocks) {
    auto *node = current_cfg_->GetNodeByBlock(exit);
    bool any = false;
    bool all_stored = true;
    bool all_carried = true;
    for (auto *predecessor : node->GetPredecessors()) {
      auto *mlir_pred = dynamic_cast<analysis::MLIRCFGNode *>(predecessor);
      if (mlir_pred == nullptr) {
        return false;
      }
      auto *pred_block = mlir_pred->GetBlock();
      any = true;
      if (StoreDominatesBlockExit(dom_tree, store, pred_block)) {
        all_carried = false;
      } else {
        all_stored = false;
      }
    }
    if (!any || (!all_stored && !all_carried)) {
      return false;  // mixed reaching values at this exit -> would need a phi (relaxation)
    }
    candidate->exits.emplace_back(exit, all_stored);
    candidate->needs_phi = candidate->needs_phi || !all_stored;
  }
  return true;
}

auto PromotionPass::Promote(analysis::Loop *loop, const Candidate &candidate, mlir::Block *preheader) -> void {
  auto *pointer = candidate.pointer;
  auto *stored_value = candidate.stored_value;

  // Build the header phi (and its preheader seed load) only if some read observes the carried
  // value; a pure overwrite needs neither.
  mlir::Value *carried = nullptr;
  if (candidate.needs_phi) {
    auto *load = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarGetValOp>(GET_MLIR_CONTEXT(), pointer);
    load->GetRes()->SetType(stored_value->GetType());
    pointer->AddUse(load, 0);
    preheader->operations_.insert(preheader->operations_.end() - 1, load);

    auto *phi_result = GET_MLIR_CONTEXT()->CreateValue();
    phi_result->SetType(stored_value->GetType());
    std::vector<mlir::Value *> values;
    std::vector<mlir::Block *> blocks;
    values.emplace_back(load->GetRes());
    blocks.emplace_back(preheader);
    for (auto *latch : loop->GetLatches()) {
      values.emplace_back(stored_value);
      blocks.emplace_back(latch);
    }
    auto *phi = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::PhiOp>(phi_result, values, blocks);
    for (size_t i = 0; i < values.size(); ++i) {
      values[i]->AddUse(phi, i);
    }
    loop->GetHeader()->operations_.insert(loop->GetHeader()->operations_.begin(), phi);
    carried = phi_result;
  }

  // Redirect each load to the value it observes, then drop it.
  for (size_t i = 0; i < candidate.loads.size(); ++i) {
    auto *load = candidate.loads[i];
    auto *value = candidate.load_uses_stored[i] ? stored_value : carried;
    if (value == nullptr) {
      throw std::logic_error("PromotionPass: carried value required but not built");
    }
    load->results_[0]->ReplaceAllUsesWith(value);
    pointer->RemoveUse(load);
    op_to_block_[load]->RemoveOperation(load);
  }

  // Drop the in-loop store; its value now flows through the phi and the exit stores.
  pointer->RemoveUse(candidate.store);
  stored_value->RemoveUse(candidate.store);
  op_to_block_[candidate.store]->RemoveOperation(candidate.store);

  // Write the location back once on each way out of the loop.
  for (const auto &exit : candidate.exits) {
    auto *exit_block = exit.first;
    auto *value = exit.second ? stored_value : carried;
    if (value == nullptr) {
      throw std::logic_error("PromotionPass: carried value required but not built");
    }
    auto *store = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarSetOp>(GET_MLIR_CONTEXT(), pointer, value);
    pointer->AddUse(store, 0);
    value->AddUse(store, 1);
    size_t insert_at = 0;
    while (insert_at < exit_block->operations_.size() && Is<mlir::base::PhiOp>(exit_block->operations_[insert_at])) {
      ++insert_at;
    }
    exit_block->operations_.insert(exit_block->operations_.begin() + insert_at, store);
  }
}

auto PromotionPass::Dominates(analysis::DomTree *dom_tree, mlir::Block *a, mlir::Block *b) -> bool {
  auto *node_a = current_cfg_->GetNodeByBlock(a);
  auto *node_b = current_cfg_->GetNodeByBlock(b);
  return node_a != nullptr && node_b != nullptr && dom_tree->Dominates(node_a, node_b);
}

auto PromotionPass::StoreDominatesOp(analysis::DomTree *dom_tree, mlir::Operation *store, mlir::Operation *op) -> bool {
  auto *store_block = op_to_block_[store];
  auto *op_block = op_to_block_[op];
  if (store_block == op_block) {
    return IndexInBlock(store_block, store) < IndexInBlock(op_block, op);
  }
  return Dominates(dom_tree, store_block, op_block);
}

auto PromotionPass::StoreDominatesBlockExit(analysis::DomTree *dom_tree, mlir::Operation *store,
                                            mlir::Block *block) -> bool {
  auto *store_block = op_to_block_[store];
  // Reaching the exit of `block`, has the store already executed? True when the store is in this
  // very block (before its terminator) or its block dominates this block.
  return store_block == block || Dominates(dom_tree, store_block, block);
}

auto PromotionPass::IsInvariant(mlir::Value *value, analysis::Loop *loop) -> bool {
  if (value == nullptr || value->op_ == nullptr) {
    return true;
  }
  auto iter = op_to_block_.find(value->op_);
  return iter == op_to_block_.end() || !loop->Contains(iter->second);
}

}  // namespace middleend::pass::transform
