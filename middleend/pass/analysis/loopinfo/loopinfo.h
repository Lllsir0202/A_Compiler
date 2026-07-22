#ifndef MIDDLEEND_PASS_ANALYSIS_LOOPINFO_LOOPINFO_H
#define MIDDLEEND_PASS_ANALYSIS_LOOPINFO_LOOPINFO_H

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "middleend/pass/analysis/analysis.h"

namespace middleend::mlir {
class Block;
}  // namespace middleend::mlir

namespace middleend::pass::analysis {
class CFGNode;
class DomTree;
class FunctionCFG;
class MLIRFunctionCFG;

/**
 * @brief A natural loop, identified by a back-edge whose target (the header) dominates its
 * source (a latch). `blocks` is the loop body: the header plus every block that can reach a
 * latch without leaving through the header. Loops sharing a header are merged, and loops are
 * nested by header containment so that LICM can process innermost loops first.
 */
class Loop {
 public:
  explicit Loop(mlir::Block *header) : header_(header) {
    blocks_.insert(header);
    ordered_blocks_.emplace_back(header);
  }

  auto GetHeader() const -> mlir::Block * { return header_; }
  // Body blocks in a deterministic order (sorted by stable block id). Iterating
  // the raw hash set instead exposes ASLR-dependent pointer order, which leaks
  // into the order downstream passes create SCEVs and base-dialect ops, making
  // the whole IR nondeterministic. Membership goes through `Contains`, so the
  // set is kept private for O(1) lookup while iteration uses this vector.
  auto GetBlocks() const -> const std::vector<mlir::Block *> & { return ordered_blocks_; }
  auto GetBlockSet() const -> const std::unordered_set<mlir::Block *> & { return blocks_; }
  auto Contains(mlir::Block *block) const -> bool { return blocks_.count(block) > 0; }
  auto GetLatches() const -> const std::vector<mlir::Block *> & { return latches_; }
  auto GetParent() const -> Loop * { return parent_; }
  auto GetSubLoops() const -> const std::vector<Loop *> & { return sub_loops_; }
  auto GetDepth() const -> int { return depth_; }

  auto AddBlock(mlir::Block *block) -> void {
    if (blocks_.insert(block).second) {
      ordered_blocks_.emplace_back(block);
    }
  }
  // Sort the body blocks into a stable order once the loop is fully built.
  auto SortBlocks() -> void;
  auto AddLatch(mlir::Block *block) -> void { latches_.emplace_back(block); }
  auto SetParent(Loop *parent) -> void { parent_ = parent; }
  auto AddSubLoop(Loop *loop) -> void { sub_loops_.emplace_back(loop); }
  auto SetDepth(int depth) -> void { depth_ = depth; }

 private:
  mlir::Block *header_;
  std::unordered_set<mlir::Block *> blocks_;
  std::vector<mlir::Block *> ordered_blocks_;  // same members as blocks_, kept in stable id order
  std::vector<mlir::Block *> latches_;
  Loop *parent_{nullptr};
  std::vector<Loop *> sub_loops_;
  int depth_{1};
};

/**
 * @brief Natural-loop detection built on the dominator tree. A CFG edge n -> h is a back-edge
 * when h dominates n; h is then a loop header. The pass groups back-edges by header, grows each
 * loop body backward from its latches, and computes loop nesting.
 */
class LoopInfoPass : public AnalysisPass {
 public:
  explicit LoopInfoPass(context::CompilerContext *compiler_context, PassStage stage)
      : AnalysisPass(compiler_context, stage) {
    AddDependency("CFGPass");
    AddDependency("DomTreePass");
  }
  ~LoopInfoPass() override { ClearState(); }

  auto Run() -> void override;
  auto Name() const -> std::string override { return "LoopInfoPass"; }
  auto IsAnalysisPass() -> bool override { return true; }

  // Every loop of a function, ordered outermost-first (a parent always precedes its children).
  auto GetLoops(FunctionCFG *function_cfg) -> const std::vector<Loop *> &;
  // The innermost loop containing `block`, or nullptr when the block is not in any loop.
  auto GetLoopFor(mlir::Block *block) -> Loop *;

 private:
  auto RunOnFunction(MLIRFunctionCFG *function_cfg, DomTree *dom_tree) -> void;
  auto BuildLoopBody(Loop *loop, MLIRFunctionCFG *function_cfg, mlir::Block *latch) -> void;
  auto ComputeNesting(const std::vector<Loop *> &loops) -> void;
  auto ClearState() -> void;

  std::vector<Loop *> owned_loops_;
  std::unordered_map<FunctionCFG *, std::vector<Loop *>> function_loops_;
  std::unordered_map<mlir::Block *, Loop *> block_to_innermost_;
  std::vector<Loop *> empty_loops_;
};

}  // namespace middleend::pass::analysis

#endif
