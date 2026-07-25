#ifndef MIDDLEEND_PASS_TRANSFORM_PROMOTION_PROMOTION_H
#define MIDDLEEND_PASS_TRANSFORM_PROMOTION_PROMOTION_H

#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
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
class DomTreeNode;
class Loop;
class LoopInfoPass;
class MLIRFunctionCFG;
}  // namespace middleend::pass::analysis

namespace middleend::pass::transform {

/**
 * @brief Scalar promotion of loop-invariant memory locations (LICM store sinking + load
 * hoisting): a location a loop repeatedly reads and writes becomes a loop-carried register --
 * one seed load in the preheader, phis where the value merges, register arithmetic in the body,
 * and one store per dedicated loop exit.
 *
 * General version: any number of stores, conditional or not. Correctness is provided by SSA
 * construction for the single location over the region (preheader + loop + exit blocks): phis
 * are placed where reaching definitions merge, a dominator-tree rename wires loads/stores/phis,
 * and each exit stores the definition reaching it. The preheader seed load makes an exit that
 * runs without any store (e.g. a zero-trip loop, or a path that skips a conditional store) write
 * the original value back -- value-preserving, and legal in single-threaded SysY with no
 * volatile. Promotion only fires when every access to the location is must-alias on a
 * loop-invariant pointer, nothing else in the loop may-alias it, and the loop has a dedicated
 * preheader and dedicated exits.
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
    AddDependency("PrePromotionAliasAnalysisPass");
    AddDependency("PrePromotionDSEPass");
    AddDependency("LoopSimplifyPass");
    AddDependency("LoopRotatePass");
    AddDependency("PrePromotionLICMPass");
  }
  ~PromotionPass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "PromotionPass"; }

 private:
  // A promotable location: a representative invariant pointer, all its must-alias stores and
  // loads in the loop, and a stored value used to type the promoted register.
  struct Candidate {
    mlir::Value *pointer{nullptr};
    mlir::Value *sample_value{nullptr};
    std::vector<mlir::Operation *> stores;
    std::vector<mlir::Operation *> loads;
  };

  auto RunOnLoop(analysis::MLIRFunctionCFG *function_cfg, analysis::DomTree *dom_tree, analysis::Loop *loop) -> bool;
  auto FindPreheader(analysis::MLIRFunctionCFG *function_cfg, analysis::Loop *loop) -> mlir::Block *;
  auto CollectExits(analysis::Loop *loop, std::vector<mlir::Block *> *exit_blocks) -> bool;
  auto TryBuildCandidate(analysis::Loop *loop, mlir::Operation *store, mlir::Block *preheader,
                         std::unordered_set<mlir::Operation *> *grouped, Candidate *candidate) -> bool;
  auto Promote(analysis::DomTree *dom_tree, analysis::Loop *loop, const Candidate &candidate, mlir::Block *preheader,
               const std::vector<mlir::Block *> &exit_blocks) -> void;
  auto Rename(analysis::DomTreeNode *dom_node, mlir::Value *reaching) -> void;
  // Reorder a function's blocks into reverse post-order so every non-phi definition precedes its
  // uses in the block list, which the LLVM lowering relies on after promotion inserts cross-block
  // stores and rewrites loads to dominating values defined in later-listed blocks.
  auto ReorderBlocksToRPO(analysis::MLIRFunctionCFG *function_cfg) -> void;

  auto IsInvariant(mlir::Value *value, analysis::Loop *loop) -> bool;
  auto Dominates(analysis::DomTree *dom_tree, mlir::Block *a, mlir::Block *b) -> bool;
  auto InRegion(mlir::Block *block) -> bool;

  analysis::AliasAnalysisPass *alias_analysis_{nullptr};
  analysis::MLIRFunctionCFG *current_cfg_{nullptr};
  std::unordered_map<mlir::Operation *, mlir::Block *> op_to_block_;

  // Per-promotion rename state (reset for each promoted location).
  mlir::Value *promoted_pointer_{nullptr};
  mlir::Block *preheader_{nullptr};
  std::unordered_set<mlir::Block *> region_;
  std::unordered_set<mlir::Block *> exit_set_;
  std::unordered_set<mlir::Operation *> promoted_stores_;
  std::unordered_set<mlir::Operation *> promoted_loads_;
  std::unordered_map<mlir::Block *, mlir::Value *> placed_phi_;
  std::unordered_map<mlir::Block *, std::vector<std::pair<mlir::Block *, mlir::Value *>>> phi_incomings_;
  std::unordered_map<mlir::Operation *, mlir::Value *> load_reaching_;
  std::unordered_map<mlir::Block *, mlir::Value *> exit_reaching_;
};

}  // namespace middleend::pass::transform

#endif
