#ifndef MIDDLEEND_PASS_TRANSFORM_DSE_DSE_H
#define MIDDLEEND_PASS_TRANSFORM_DSE_DSE_H

#include <cstddef>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
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
class MLIRFunctionCFG;
}  // namespace middleend::pass::analysis

namespace middleend::pass::transform {

/**
 * @brief Dead store elimination backed by @ref AliasAnalysisPass.
 *
 * Removes two kinds of provably useless stores, without ever weakening correctness:
 *  - Overwritten stores: a store to `p` is dead when, on every forward CFG path from it,
 *    the location is overwritten by a must-alias store before it can be read (by a may-alias
 *    load or a call that may reference it) and before any function exit. A may-alias load or
 *    may-ref call ends the store's dead window on that path. This is a full control-flow-graph
 *    reachability query, so it removes cross-block dead stores (killed on all successor paths),
 *    not just consecutive stores inside one block.
 *  - Write-only locals: a store to a local whose address never escapes and which is never
 *    loaded anywhere in the function is unobservable and removed.
 *
 * Both rules only fire when the store is guaranteed unobservable, so this stays general and
 * correct without per-input special-casing.
 */
class DSEPass : public TransformPass {
 public:
  DSEPass(context::CompilerContext *compiler_context, PassStage stage, bool before_promotion = false)
      : TransformPass(compiler_context, stage), before_promotion_(before_promotion) {
    if (stage != PassStage::BASEIR) {
      throw std::invalid_argument("DSEPass only supports BASEIR stage");
    }
    AddDependency("CFGPass");
    AddDependency(before_promotion ? "PrePromotionAliasAnalysisPass" : "AliasAnalysisPass");
  }
  ~DSEPass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "DSEPass"; }

 private:
  auto ComputeLoadedLocals() -> void;
  auto RunOnFunction(analysis::MLIRFunctionCFG *function_cfg) -> bool;
  auto IsWriteOnlyDead(mlir::Value *pointer) -> bool;
  // A store to `pointer` at position `store_index` in `block` is dead when every forward path
  // overwrites the location (must-alias store) before reading it or exiting the function.
  auto IsDeadByOverwrite(mlir::Block *block, size_t store_index, mlir::Value *pointer) -> bool;
  // Scan `block` from `begin` onward; true if this path segment plus all its successors are
  // overwritten before any read/exit. `on_stack` guards back-edges, `memo` caches block entries.
  auto TailOverwritten(mlir::Block *block, size_t begin, mlir::Value *pointer,
                       std::unordered_set<mlir::Block *> *on_stack,
                       std::unordered_map<mlir::Block *, bool> *memo) -> bool;
  auto BlockEntryOverwritten(mlir::Block *block, mlir::Value *pointer, std::unordered_set<mlir::Block *> *on_stack,
                             std::unordered_map<mlir::Block *, bool> *memo) -> bool;
  auto RemoveStores(mlir::Block *block, const std::unordered_set<mlir::Operation *> &dead) -> void;

  analysis::AliasAnalysisPass *alias_analysis_{nullptr};
  analysis::MLIRFunctionCFG *current_cfg_{nullptr};
  std::unordered_set<mlir::Value *> loaded_local_roots_;
  bool before_promotion_{false};
};

class PrePromotionDSEPass : public DSEPass {
 public:
  PrePromotionDSEPass(context::CompilerContext *compiler_context, PassStage stage)
      : DSEPass(compiler_context, stage, true) {}
  ~PrePromotionDSEPass() override = default;

  auto Name() const -> std::string override { return "PrePromotionDSEPass"; }
};

}  // namespace middleend::pass::transform

#endif
