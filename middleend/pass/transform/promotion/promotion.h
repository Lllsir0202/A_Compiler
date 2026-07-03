#ifndef MIDDLEEND_PASS_TRANSFORM_PROMOTION_PROMOTION_H
#define MIDDLEEND_PASS_TRANSFORM_PROMOTION_PROMOTION_H

#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>
#include "middleend/pass/transform/transform.h"

namespace middleend::mlir {
class Block;
class Operation;
class Value;
}  // namespace middleend::mlir

namespace middleend::pass::analysis {
class AliasAnalysisPass;
class DomTree;
class Loop;
class LoopInfoPass;
class MLIRFunctionCFG;
}  // namespace middleend::pass::analysis

namespace middleend::pass::transform {

/**
 * @brief Scalar promotion of loop-invariant memory locations (LICM store sinking + load
 * hoisting), turning a location that a loop repeatedly reads and writes into a loop-carried
 * register value: one load in the preheader, a header phi, register arithmetic in the body,
 * and one store per loop exit.
 *
 * Safe subset (v1): promote a location `p` only when
 *   - all loop accesses to `p` use a loop-invariant, must-alias pointer;
 *   - exactly one store writes `p`, and its block dominates every latch (so the value carried
 *     across the back-edge is always that store's value -- a single header phi suffices);
 *   - nothing else in the loop may-alias `p` (no may-alias load/store, no mod/ref call);
 *   - every block holding such a load, and every loop-exiting block, is dominator-comparable to
 *     the store's block (no internal merge of the stored value with the carried value);
 *   - the loop has a dedicated preheader and dedicated exits.
 * The preheader load seeds the header phi, so on a zero-trip loop the exit store writes the
 * original value back -- value-preserving, and legal in single-threaded SysY with no volatile.
 * Conditional/multiple-store promotion (needing general phi insertion) is left as future work.
 */
class PromotionPass : public TransformPass {
 public:
  PromotionPass(context::CompilerContext *compiler_context, PassStage stage) : TransformPass(compiler_context, stage) {
    if (stage != PassStage::BASEIR) {
      throw std::invalid_argument("PromotionPass only supports BASEIR stage");
    }
    AddDependency("CFGPass");
    AddDependency("DomTreePass");
    AddDependency("LoopInfoPass");
    AddDependency("AliasAnalysisPass");
    AddDependency("LoopSimplifyPass");
  }
  ~PromotionPass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "PromotionPass"; }

 private:
  // One validated promotion: the single store, its location and stored value, the must-alias
  // loads (each tagged with whether it reads the stored value or the carried phi), the exit
  // stores to insert (each tagged likewise), and whether a header phi is needed.
  struct Candidate {
    mlir::Operation *store{nullptr};
    mlir::Value *pointer{nullptr};
    mlir::Value *stored_value{nullptr};
    std::vector<mlir::Operation *> loads;
    std::vector<bool> load_uses_stored;
    std::vector<std::pair<mlir::Block *, bool>> exits;  // (exit block, uses stored value)
    bool needs_phi{false};
  };

  auto RunOnLoop(analysis::MLIRFunctionCFG *function_cfg, analysis::DomTree *dom_tree, analysis::Loop *loop) -> bool;
  auto FindPreheader(analysis::MLIRFunctionCFG *function_cfg, analysis::Loop *loop) -> mlir::Block *;
  auto CollectExits(analysis::Loop *loop, std::vector<mlir::Block *> *exit_blocks) -> bool;
  auto TryBuildCandidate(analysis::DomTree *dom_tree, analysis::Loop *loop, mlir::Operation *store,
                         mlir::Block *preheader, const std::vector<mlir::Block *> &exit_blocks,
                         Candidate *candidate) -> bool;
  auto Promote(analysis::Loop *loop, const Candidate &candidate, mlir::Block *preheader) -> void;

  auto Dominates(analysis::DomTree *dom_tree, mlir::Block *a, mlir::Block *b) -> bool;
  auto StoreDominatesOp(analysis::DomTree *dom_tree, mlir::Operation *store, mlir::Operation *op) -> bool;
  auto StoreDominatesBlockExit(analysis::DomTree *dom_tree, mlir::Operation *store, mlir::Block *block) -> bool;
  auto IsInvariant(mlir::Value *value, analysis::Loop *loop) -> bool;

  analysis::AliasAnalysisPass *alias_analysis_{nullptr};
  analysis::MLIRFunctionCFG *current_cfg_{nullptr};
  std::unordered_map<mlir::Operation *, mlir::Block *> op_to_block_;
};

}  // namespace middleend::pass::transform

#endif
