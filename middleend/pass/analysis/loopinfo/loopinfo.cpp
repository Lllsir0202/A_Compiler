#include "middleend/pass/analysis/loopinfo/loopinfo.h"
#include <algorithm>
#include <stdexcept>
#include "context/compiler_context.h"
#include "middleend/mlir/utils/struct/block.h"
#include "middleend/pass/analysis/cfg/cfg.h"
#include "middleend/pass/analysis/cfg/mlircfg.h"
#include "middleend/pass/analysis/domtree/domtree.h"

#define GET_PASS_CONTEXT() compiler_context_->GetPassContext()

namespace middleend::pass::analysis {
namespace {
auto BlockOf(CFGNode *node) -> mlir::Block * {
  auto *mlir_node = dynamic_cast<MLIRCFGNode *>(node);
  return mlir_node == nullptr ? nullptr : mlir_node->GetBlock();
}
}  // namespace

auto LoopInfoPass::Run() -> void {
  PreRun();
  ClearState();
  auto *cfg_pass = GET_PASS_CONTEXT()->GetSpecPass<CFGPass>("CFGPass");
  auto *dom_tree_pass = GET_PASS_CONTEXT()->GetSpecPass<DomTreePass>("DomTreePass");
  if (cfg_pass == nullptr || dom_tree_pass == nullptr) {
    throw std::logic_error("LoopInfoPass requires CFGPass and DomTreePass");
  }

  for (auto *function_cfg : cfg_pass->Get()) {
    auto *mlir_function_cfg = dynamic_cast<MLIRFunctionCFG *>(function_cfg);
    if (mlir_function_cfg == nullptr) {
      throw std::logic_error("LoopInfoPass only supports MLIRFunctionCFG");
    }
    auto *dom_tree = dom_tree_pass->GetDomTree(function_cfg);
    if (dom_tree == nullptr) {
      throw std::logic_error("LoopInfoPass requires the dominator tree of every function");
    }
    RunOnFunction(mlir_function_cfg, dom_tree);
  }
  Validate();
}

auto LoopInfoPass::RunOnFunction(MLIRFunctionCFG *function_cfg, DomTree *dom_tree) -> void {
  // A back-edge n -> h has the header h dominating its source n. Group latches by header.
  std::unordered_map<mlir::Block *, Loop *> header_to_loop;
  std::vector<Loop *> loops;
  for (auto *node : function_cfg->GetNodes()) {
    for (auto *successor : node->GetSuccessors()) {
      if (!dom_tree->Dominates(successor, node)) {
        continue;
      }
      auto *header = BlockOf(successor);
      auto *latch = BlockOf(node);
      if (header == nullptr || latch == nullptr) {
        continue;
      }
      auto iter = header_to_loop.find(header);
      if (iter == header_to_loop.end()) {
        auto *loop = new Loop(header);
        owned_loops_.emplace_back(loop);
        header_to_loop[header] = loop;
        loops.emplace_back(loop);
        iter = header_to_loop.find(header);
      }
      iter->second->AddLatch(latch);
      BuildLoopBody(iter->second, function_cfg, latch);
    }
  }

  // Put each loop body into a deterministic order before any consumer iterates it.
  for (auto *loop : loops) {
    loop->SortBlocks();
  }

  ComputeNesting(loops);
  function_loops_[function_cfg] = loops;

  // Map every block to the innermost (smallest) loop that contains it.
  for (auto *loop : loops) {
    for (auto *block : loop->GetBlocks()) {
      auto existing = block_to_innermost_.find(block);
      if (existing == block_to_innermost_.end() || loop->GetBlocks().size() < existing->second->GetBlocks().size()) {
        block_to_innermost_[block] = loop;
      }
    }
  }
}

auto LoopInfoPass::BuildLoopBody(Loop *loop, MLIRFunctionCFG *function_cfg, mlir::Block *latch) -> void {
  // Walk predecessors backward from the latch, stopping at the header, collecting the body.
  std::vector<mlir::Block *> worklist;
  worklist.emplace_back(latch);
  while (!worklist.empty()) {
    auto *block = worklist.back();
    worklist.pop_back();
    if (loop->Contains(block)) {
      continue;  // header is pre-inserted, so its predecessors are never traversed
    }
    loop->AddBlock(block);
    auto *node = function_cfg->GetNodeByBlock(block);
    if (node == nullptr) {
      continue;
    }
    for (auto *predecessor : node->GetPredecessors()) {
      auto *pred_block = BlockOf(predecessor);
      if (pred_block != nullptr && !loop->Contains(pred_block)) {
        worklist.emplace_back(pred_block);
      }
    }
  }
}

auto Loop::SortBlocks() -> void {
  std::sort(ordered_blocks_.begin(), ordered_blocks_.end(),
            [](const mlir::Block *a, const mlir::Block *b) { return a->block_id_ < b->block_id_; });
}

auto LoopInfoPass::ComputeNesting(const std::vector<Loop *> &loops) -> void {
  for (auto *inner : loops) {
    Loop *parent = nullptr;
    for (auto *outer : loops) {
      if (outer == inner || !outer->Contains(inner->GetHeader())) {
        continue;
      }
      // The parent is the smallest loop that strictly contains this one's header.
      if (parent == nullptr || outer->GetBlocks().size() < parent->GetBlocks().size()) {
        parent = outer;
      }
    }
    if (parent != nullptr) {
      inner->SetParent(parent);
      parent->AddSubLoop(inner);
    }
  }
  // Depth = number of enclosing loops + 1; parents are always larger, so a size-descending
  // pass sets every parent's depth before its children.
  std::vector<Loop *> ordered = loops;
  std::sort(ordered.begin(), ordered.end(),
            [](Loop *lhs, Loop *rhs) { return lhs->GetBlocks().size() > rhs->GetBlocks().size(); });
  for (auto *loop : ordered) {
    loop->SetDepth(loop->GetParent() == nullptr ? 1 : loop->GetParent()->GetDepth() + 1);
  }
}

auto LoopInfoPass::GetLoops(FunctionCFG *function_cfg) -> const std::vector<Loop *> & {
  auto iter = function_loops_.find(function_cfg);
  return iter == function_loops_.end() ? empty_loops_ : iter->second;
}

auto LoopInfoPass::GetLoopFor(mlir::Block *block) -> Loop * {
  auto iter = block_to_innermost_.find(block);
  return iter == block_to_innermost_.end() ? nullptr : iter->second;
}

auto LoopInfoPass::ClearState() -> void {
  for (auto *loop : owned_loops_) {
    delete loop;
  }
  owned_loops_.clear();
  function_loops_.clear();
  block_to_innermost_.clear();
}

}  // namespace middleend::pass::analysis
