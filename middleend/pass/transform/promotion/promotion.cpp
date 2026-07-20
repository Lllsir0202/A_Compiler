#include "middleend/pass/transform/promotion/promotion.h"
#include <algorithm>
#include <stdexcept>
#include <utility>
#include <vector>
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/base/funccall.h"
#include "middleend/mlir/dialect/base/funcdef.h"
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
auto BlockOf(analysis::CFGNode *node) -> mlir::Block * {
  auto *mlir_node = dynamic_cast<analysis::MLIRCFGNode *>(node);
  return mlir_node == nullptr ? nullptr : mlir_node->GetBlock();
}
auto StoredValue(mlir::Operation *store) -> mlir::Value * {
  return mlir::DynamicOrNull<mlir::var::VarSetOp>(store)->GetNewValue();
}
auto StorePtr(mlir::Operation *store) -> mlir::Value * {
  return mlir::DynamicOrNull<mlir::var::VarSetOp>(store)->GetPtr();
}
auto LoadPtr(mlir::Operation *load) -> mlir::Value * {
  return mlir::DynamicOrNull<mlir::var::VarGetValOp>(load)->GetPtr();
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
      auto *block = BlockOf(cfg_node);
      if (block == nullptr) {
        continue;
      }
      for (auto *operation : block->operations_) {
        op_to_block_[operation] = block;
      }
    }
    bool function_changed = false;
    for (auto *loop : loop_info->GetLoops(function_cfg)) {
      function_changed = RunOnLoop(mlir_function_cfg, dom_tree, loop) || function_changed;
    }
    if (function_changed) {
      ReorderBlocksToRPO(mlir_function_cfg);
      changed = true;
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

  // Validate every location against the original loop, grouping the stores that write it, then
  // apply. Promotable locations are mutually no-alias, so applying one never invalidates another.
  std::vector<Candidate> candidates;
  std::unordered_set<mlir::Operation *> grouped;
  for (auto *block : loop->GetBlocks()) {
    for (auto *operation : block->operations_) {
      if (!IsStore(operation) || grouped.count(operation) > 0) {
        continue;
      }
      Candidate candidate;
      if (TryBuildCandidate(loop, operation, preheader, &grouped, &candidate)) {
        candidates.emplace_back(std::move(candidate));
      }
    }
  }

  for (const auto &candidate : candidates) {
    Promote(dom_tree, loop, candidate, preheader, exit_blocks);
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
    auto *pred_block = BlockOf(predecessor);
    if (pred_block == nullptr || loop->Contains(pred_block)) {
      continue;
    }
    if (preheader != nullptr) {
      return nullptr;
    }
    preheader = pred_block;
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
      auto *succ_block = BlockOf(successor);
      if (succ_block == nullptr || loop->Contains(succ_block)) {
        continue;
      }
      if (std::find(exits.begin(), exits.end(), succ_block) == exits.end()) {
        exits.emplace_back(succ_block);
      }
    }
  }
  // Dedicated exits: every predecessor of an exit block lies inside the loop, so a store inserted
  // there runs only when leaving the loop.
  for (auto *exit : exits) {
    auto *node = current_cfg_->GetNodeByBlock(exit);
    if (node == nullptr) {
      return false;
    }
    for (auto *predecessor : node->GetPredecessors()) {
      auto *pred_block = BlockOf(predecessor);
      if (pred_block == nullptr || !loop->Contains(pred_block)) {
        return false;
      }
    }
  }
  *exit_blocks = std::move(exits);
  return true;
}

auto PromotionPass::TryBuildCandidate(analysis::Loop *loop, mlir::Operation *store, mlir::Block *preheader,
                                      std::unordered_set<mlir::Operation *> *grouped, Candidate *candidate) -> bool {
  auto *pointer = StorePtr(store);
  if (!IsInvariant(pointer, loop)) {
    return false;
  }
  // The preheader seed load reads `pointer`, so its definition must dominate the preheader.
  if (pointer->op_ != nullptr) {
    auto iter = op_to_block_.find(pointer->op_);
    if (iter != op_to_block_.end() && current_cfg_->GetNodeByBlock(iter->second) != nullptr) {
      auto *dom_tree = GET_PASS_CONTEXT()->GetSpecPass<analysis::DomTreePass>("DomTreePass")->GetDomTree(current_cfg_);
      if (dom_tree != nullptr && !Dominates(dom_tree, iter->second, preheader)) {
        return false;
      }
    }
  }

  // Gather every must-alias store/load of this location; reject if anything else may-alias it.
  std::vector<mlir::Operation *> stores;
  std::vector<mlir::Operation *> loads;
  for (auto *block : loop->GetBlocks()) {
    for (auto *operation : block->operations_) {
      if (IsStore(operation)) {
        auto result = alias_analysis_->Alias(pointer, StorePtr(operation));
        if (result == analysis::AliasResult::MustAlias) {
          stores.emplace_back(operation);
        } else if (result != analysis::AliasResult::NoAlias) {
          return false;
        }
      } else if (IsLoad(operation)) {
        auto result = alias_analysis_->Alias(pointer, LoadPtr(operation));
        if (result == analysis::AliasResult::MustAlias) {
          loads.emplace_back(operation);
        } else if (result != analysis::AliasResult::NoAlias) {
          return false;
        }
      } else if (IsCall(operation) && alias_analysis_->CallModRefPrecise(operation, pointer)) {
        return false;
      }
    }
  }
  // A stored value that is itself a promoted load (`p = p`) would be rewritten under itself.
  for (auto *candidate_store : stores) {
    if (std::find(loads.begin(), loads.end(), StoredValue(candidate_store)->op_) != loads.end()) {
      return false;
    }
  }

  for (auto *candidate_store : stores) {
    grouped->insert(candidate_store);
  }
  candidate->pointer = pointer;
  candidate->sample_value = StoredValue(store);
  candidate->stores = std::move(stores);
  candidate->loads = std::move(loads);
  return true;
}

auto PromotionPass::Promote(analysis::DomTree *dom_tree, analysis::Loop *loop, const Candidate &candidate,
                            mlir::Block *preheader, const std::vector<mlir::Block *> &exit_blocks) -> void {
  // Reset per-location rename state and define the region (preheader + loop + exit blocks).
  promoted_pointer_ = candidate.pointer;
  preheader_ = preheader;
  region_.clear();
  exit_set_.clear();
  promoted_stores_.clear();
  promoted_loads_.clear();
  placed_phi_.clear();
  phi_incomings_.clear();
  load_reaching_.clear();
  exit_reaching_.clear();

  region_.insert(preheader);
  for (auto *block : loop->GetBlocks()) {
    region_.insert(block);
  }
  for (auto *exit : exit_blocks) {
    region_.insert(exit);
    exit_set_.insert(exit);
  }
  for (auto *store : candidate.stores) {
    promoted_stores_.insert(store);
  }
  for (auto *load : candidate.loads) {
    promoted_loads_.insert(load);
  }

  auto *value_type = candidate.sample_value->GetType();

  // Place a phi at every region block (except the preheader entry) that merges control flow.
  for (auto *block : region_) {
    if (block == preheader) {
      continue;
    }
    auto *node = current_cfg_->GetNodeByBlock(block);
    size_t region_preds = 0;
    for (auto *predecessor : node->GetPredecessors()) {
      if (region_.count(BlockOf(predecessor)) > 0) {
        ++region_preds;
      }
    }
    if (region_preds >= 2) {
      auto *phi_value = GET_MLIR_CONTEXT()->CreateValue();
      phi_value->SetType(value_type);
      placed_phi_[block] = phi_value;
    }
  }

  // Seed load: created now (so its value can seed the rename) but inserted after renaming.
  auto *seed_load = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarGetValOp>(GET_MLIR_CONTEXT(), promoted_pointer_);
  seed_load->GetRes()->SetType(value_type);

  auto *entry_node = dom_tree->GetDomTreeNode(current_cfg_->GetNodeByBlock(preheader));
  if (entry_node != nullptr) {
    Rename(entry_node, seed_load->GetRes());
  }

  // Materialize: insert the seed load, build the phis, redirect loads, drop stores, add exit stores.
  preheader->operations_.insert(preheader->operations_.end() - 1, seed_load);
  promoted_pointer_->AddUse(seed_load, 0);
  // Every operation this pass splices in must be registered: an enclosing loop is
  // promoted after the loops nested in it, and its candidate loads/stores can be
  // ops created here (a seed load in a preheader, a sunk store in an exit block).
  op_to_block_[seed_load] = preheader;

  std::vector<std::pair<mlir::Operation *, mlir::Block *>> created_phis;
  for (auto &entry : placed_phi_) {
    auto *block = entry.first;
    auto &incomings = phi_incomings_[block];
    std::vector<mlir::Value *> values;
    std::vector<mlir::Block *> blocks;
    for (auto &incoming : incomings) {
      blocks.emplace_back(incoming.first);
      values.emplace_back(incoming.second);
    }
    auto *phi = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::PhiOp>(entry.second, values, blocks);
    for (size_t i = 0; i < values.size(); ++i) {
      values[i]->AddUse(phi, i);
    }
    block->operations_.insert(block->operations_.begin(), phi);
    op_to_block_[phi] = block;
    created_phis.emplace_back(phi, block);
  }

  for (auto *load : candidate.loads) {
    load->results_[0]->ReplaceAllUsesWith(load_reaching_[load]);
    promoted_pointer_->RemoveUse(load);
    op_to_block_[load]->RemoveOperation(load);
    op_to_block_.erase(load);
  }
  for (auto *store : candidate.stores) {
    promoted_pointer_->RemoveUse(store);
    StoredValue(store)->RemoveUse(store);
    op_to_block_[store]->RemoveOperation(store);
    op_to_block_.erase(store);
  }
  for (auto *exit : exit_blocks) {
    auto *value = exit_reaching_[exit];
    auto *store =
        GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarSetOp>(GET_MLIR_CONTEXT(), promoted_pointer_, value);
    promoted_pointer_->AddUse(store, 0);
    value->AddUse(store, 1);
    size_t insert_at = 0;
    while (insert_at < exit->operations_.size() && Is<mlir::base::PhiOp>(exit->operations_[insert_at])) {
      ++insert_at;
    }
    exit->operations_.insert(exit->operations_.begin() + insert_at, store);
    op_to_block_[store] = exit;
  }

  // Remove trivial phis (all incomings equal, ignoring self-references), cascading.
  std::unordered_set<mlir::Operation *> removed;
  bool changed = true;
  while (changed) {
    changed = false;
    for (auto &entry : created_phis) {
      auto *phi = mlir::DynamicOrNull<mlir::base::PhiOp>(entry.first);
      if (phi == nullptr || removed.count(entry.first) > 0) {
        continue;
      }
      mlir::Value *unique = nullptr;
      bool multiple = false;
      for (auto *value : phi->GetIncomingValue()) {
        if (value == phi->GetRes()) {
          continue;
        }
        if (unique == nullptr) {
          unique = value;
        } else if (unique != value) {
          multiple = true;
          break;
        }
      }
      if (multiple || unique == nullptr) {
        continue;
      }
      phi->GetRes()->ReplaceAllUsesWith(unique);
      for (auto *value : phi->GetIncomingValue()) {
        value->RemoveUse(phi);
      }
      entry.second->RemoveOperation(phi);
      removed.insert(entry.first);
      changed = true;
    }
  }

  // Drop the seed load if promotion left it unused.
  if (seed_load->GetRes()->GetUses().empty()) {
    promoted_pointer_->RemoveUse(seed_load);
    preheader->RemoveOperation(seed_load);
  }
}

auto PromotionPass::Rename(analysis::DomTreeNode *dom_node, mlir::Value *reaching) -> void {
  auto *block = BlockOf(dom_node->GetCFGNode());
  if (block == nullptr || !InRegion(block)) {
    return;  // region blocks never sit below a non-region block in the dominator tree
  }
  mlir::Value *current = reaching;
  auto phi_iter = placed_phi_.find(block);
  if (phi_iter != placed_phi_.end()) {
    current = phi_iter->second;
  }
  if (exit_set_.count(block) > 0) {
    exit_reaching_[block] = current;
    return;  // exits are leaves: the store is inserted, they hold no promoted access
  }

  for (auto *operation : block->operations_) {
    if (promoted_loads_.count(operation) > 0) {
      load_reaching_[operation] = current;
    } else if (promoted_stores_.count(operation) > 0) {
      current = StoredValue(operation);
    }
  }

  for (auto *successor : dom_node->GetCFGNode()->GetSuccessors()) {
    auto *succ_block = BlockOf(successor);
    if (succ_block != nullptr && placed_phi_.count(succ_block) > 0) {
      phi_incomings_[succ_block].emplace_back(block, current);
    }
  }
  for (auto *child : dom_node->GetChildren()) {
    Rename(child, current);
  }
}

auto PromotionPass::ReorderBlocksToRPO(analysis::MLIRFunctionCFG *function_cfg) -> void {
  auto *func_op = function_cfg->GetFuncOp();
  auto *entry = func_op->GetEntryBlock();
  auto *entry_node = function_cfg->GetNodeByBlock(entry);
  if (entry_node == nullptr) {
    return;
  }

  // Iterative post-order DFS over the CFG (edges are unchanged by promotion), then reverse.
  std::vector<mlir::Block *> post_order;
  std::unordered_set<analysis::CFGNode *> visited;
  std::vector<std::pair<analysis::CFGNode *, size_t>> stack;
  visited.insert(entry_node);
  stack.emplace_back(entry_node, 0);
  while (!stack.empty()) {
    auto &frame = stack.back();
    auto successors = frame.first->GetSuccessors();
    if (frame.second < successors.size()) {
      auto *child = successors[frame.second++];
      if (visited.insert(child).second) {
        stack.emplace_back(child, 0);
      }
    } else {
      post_order.emplace_back(BlockOf(frame.first));
      stack.pop_back();
    }
  }

  std::vector<mlir::Block *> ordered(post_order.rbegin(), post_order.rend());
  // Keep any block unreachable from entry (not in the CFG walk) at the end, preserving order.
  std::unordered_set<mlir::Block *> placed(ordered.begin(), ordered.end());
  for (auto *block : func_op->GetBlocks()) {
    if (placed.count(block) == 0) {
      ordered.emplace_back(block);
    }
  }
  func_op->SetBlocks(std::move(ordered));
}

auto PromotionPass::IsInvariant(mlir::Value *value, analysis::Loop *loop) -> bool {
  if (value == nullptr || value->op_ == nullptr) {
    return true;
  }
  auto iter = op_to_block_.find(value->op_);
  return iter == op_to_block_.end() || !loop->Contains(iter->second);
}

auto PromotionPass::Dominates(analysis::DomTree *dom_tree, mlir::Block *a, mlir::Block *b) -> bool {
  auto *node_a = current_cfg_->GetNodeByBlock(a);
  auto *node_b = current_cfg_->GetNodeByBlock(b);
  return node_a != nullptr && node_b != nullptr && dom_tree->Dominates(node_a, node_b);
}

auto PromotionPass::InRegion(mlir::Block *block) -> bool { return region_.count(block) > 0; }

}  // namespace middleend::pass::transform
