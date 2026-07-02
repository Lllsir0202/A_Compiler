#ifndef MIDDLEEND_PASS_TRANSFORM_DSE_DSE_H
#define MIDDLEEND_PASS_TRANSFORM_DSE_DSE_H

#include <stdexcept>
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
 *  - Overwritten stores: within a block, a store is dead when a later must-alias store
 *    overwrites it and nothing in between may read the location (no may-alias load and no
 *    call that may reference it). A may-alias load or call ends a store's dead window.
 *  - Write-only locals: a store to a local whose address never escapes and which is never
 *    loaded anywhere in the function is unobservable and removed.
 *
 * Both rules only fire when the store is guaranteed unobservable, so this stays general and
 * correct without per-input special-casing. Cross-block overwrite elimination (via memory
 * SSA / post-dominance) is left as future work.
 */
class DSEPass : public TransformPass {
 public:
  DSEPass(context::CompilerContext *compiler_context, PassStage stage) : TransformPass(compiler_context, stage) {
    if (stage != PassStage::BASEIR) {
      throw std::invalid_argument("DSEPass only supports BASEIR stage");
    }
    AddDependency("CFGPass");
    AddDependency("AliasAnalysisPass");
  }
  ~DSEPass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "DSEPass"; }

 private:
  auto ComputeLoadedLocals() -> void;
  auto RunOnBlock(mlir::Block *block) -> bool;
  auto IsWriteOnlyDead(mlir::Value *pointer) -> bool;
  auto RemoveStores(mlir::Block *block, const std::unordered_set<mlir::Operation *> &dead) -> void;

  analysis::AliasAnalysisPass *alias_analysis_{nullptr};
  std::unordered_set<mlir::Value *> loaded_local_roots_;
  bool changed_{false};
};

}  // namespace middleend::pass::transform

#endif
