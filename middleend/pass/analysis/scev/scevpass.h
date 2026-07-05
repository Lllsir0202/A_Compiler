#ifndef MIDDLEEND_PASS_ANALYSIS_SCEV_SCEVPASS_H
#define MIDDLEEND_PASS_ANALYSIS_SCEV_SCEVPASS_H
#include <string>
#include <unordered_map>
#include <vector>
#include "middleend/pass/analysis/analysis.h"
#include "middleend/pass/analysis/scev/scev.h"

namespace middleend::mlir {
class Value;
class Block;
class Operation;
}  // namespace middleend::mlir
namespace middleend::mlir::base {
class PhiOp;
}

namespace middleend::pass::analysis {
class Loop;
class LoopInfoPass;
class CFGPass;
class MLIRFunctionCFG;

/**
 * @brief Scalar-evolution analysis over BASEIR. It lazily maps each Value to a uniqued SCEV,
 * recognizes affine (and nested) induction variables from loop-header phis, and derives
 * backedge-taken counts from affine exit conditions.
 *
 * Rich expressions are built only for integer values. Float values stay opaque (SCEVUnknown) so no
 * reasoning ever depends on IEEE rounding, and pointers/loads/calls are opaque leaves as well. Every
 * fold is exact under 32-bit modular arithmetic, so recurrence construction needs no no-wrap
 * assumption; only backedge-count derivation assumes the canonical monotonic form does not wrap,
 * which is isolated in ComputeBackedgeTakenCount.
 *
 * The pass owns every node in owned_scevs_ and releases them in ClearState(), mirroring how
 * LoopInfoPass owns its Loop objects.
 */
class SCEVPass : public AnalysisPass {
 public:
  SCEVPass(context::CompilerContext *compiler_context, PassStage stage) : AnalysisPass(compiler_context, stage) {
    AddDependency("CFGPass");
    AddDependency("DomTreePass");
    AddDependency("LoopInfoPass");
    AddDependency("LoopSimplifyPass");
  }
  ~SCEVPass() override { ClearState(); }

  auto Run() -> void override;
  auto Name() const -> std::string override { return "SCEVPass"; }

  // ---- Query surface every consumer codes against ----------------------------------------------

  /** @brief The SCEV of a value, built and memoized on demand. Never returns null. */
  auto GetSCEV(mlir::Value *value) -> const SCEV *;

  /** @brief True when `scev` does not vary inside `loop`. */
  auto IsLoopInvariant(const SCEV *scev, Loop *loop) -> bool;

  /** @brief The loop that immediately contains `block`, or nullptr when it is outside every loop. */
  auto GetLoopFor(mlir::Block *block) -> Loop *;

  /** @brief The block that defines `value`, or nullptr for parameters/globals. */
  auto GetBlockOf(mlir::Value *value) -> mlir::Block *;

  /** @brief Times the backedge is taken; a SCEVUnknown sentinel when it cannot be computed. */
  auto GetBackedgeTakenCount(Loop *loop) -> const SCEV *;

  /** @brief Constant trip count (backedge-taken + 1) when statically known, else -1. */
  auto GetConstantTripCount(Loop *loop) -> int64_t;

  // ---- Builders reused by ComputeBackedgeTakenCount and by consumers ---------------------------
  auto GetConstant(int value) -> const SCEV *;
  auto GetUnknown(mlir::Value *value) -> const SCEV *;
  auto GetAddExpr(std::vector<const SCEV *> operands) -> const SCEV *;
  auto GetMulExpr(std::vector<const SCEV *> operands) -> const SCEV *;
  auto GetDivExpr(const SCEV *lhs, const SCEV *rhs) -> const SCEV *;
  auto GetSMaxExpr(std::vector<const SCEV *> operands) -> const SCEV *;
  auto GetUMinExpr(std::vector<const SCEV *> operands) -> const SCEV *;
  // Affine recurrence {start, +, step}; the chain form is built internally from a vector.
  auto GetAddRecExpr(const SCEV *start, const SCEV *step, Loop *loop) -> const SCEV *;
  auto GetAddRecExpr(std::vector<const SCEV *> operands, Loop *loop, bool no_wrap = false) -> const SCEV *;
  auto GetMinusSCEV(const SCEV *lhs, const SCEV *rhs) -> const SCEV *;
  auto GetNegativeSCEV(const SCEV *scev) -> const SCEV *;

  // The value of `scev` on iteration `iteration` (Newton-series evaluation of a chain of
  // recurrences), used to close a loop into its final value. Returns nullptr when it cannot be
  // expressed (e.g. a non-integer division that does not evaluate exactly).
  auto EvaluateAtIteration(const SCEV *scev, const SCEV *iteration) -> const SCEV *;

 private:
  auto RunOnFunction(MLIRFunctionCFG *function_cfg) -> void;

  // Value -> SCEV, dispatched on the defining op. A temporary self-symbol is memoized before
  // recursion so a self-referential header phi converges instead of recursing forever.
  auto CreateSCEV(mlir::Value *value) -> const SCEV *;
  auto CreateSCEVForOperation(mlir::Operation *op, mlir::Value *result) -> const SCEV *;
  auto CreateAddRecFromPhi(mlir::base::PhiOp *phi, Loop *loop) -> const SCEV *;

  auto ComputeBackedgeTakenCount(Loop *loop) -> const SCEV *;
  auto ComputeExitCount(Loop *loop, mlir::Block *exiting) -> const SCEV *;
  auto GetConstantValue(const SCEV *scev, int *out) -> bool;

  // Decompose an Add operand into a coefficient and a base term so like terms can be merged:
  // 3*x has coefficient 3 and base x; a bare term has coefficient 1.
  auto SplitAffineTerm(const SCEV *scev, const SCEV **base) -> int;
  auto GetBinomialCoefficient(const SCEV *iteration, int order) -> const SCEV *;

  // True when `scev` structurally contains `target` (pointer identity of a uniqued node).
  auto ScevContains(const SCEV *scev, const SCEV *target) -> bool;
  // Drop every memoized value whose SCEV still references `symbol`. Used after a header phi resolves
  // to its recurrence, to discard the placeholder-based results computed for values along the
  // back-edge (e.g. i+1) so they are recomputed against the finished recurrence.
  auto ForgetValuesContaining(const SCEV *symbol) -> void;

  // Structural uniquing: an equal candidate returns the previously created node; otherwise the
  // candidate is adopted into owned_scevs_ and returned.
  auto Unique(SCEV *candidate) -> const SCEV *;
  auto IntType() -> type::Type *;
  auto DefinedInLoop(mlir::Value *value, Loop *loop) -> bool;
  auto ClearState() -> void;

  std::vector<SCEV *> owned_scevs_;
  std::unordered_map<std::string, SCEV *> unique_map_;
  std::unordered_map<mlir::Value *, const SCEV *> value_to_scev_;
  std::unordered_map<Loop *, const SCEV *> backedge_taken_;
  std::unordered_map<mlir::Operation *, mlir::Block *> op_to_block_;
  LoopInfoPass *loop_info_{nullptr};
  type::Type *int_type_{nullptr};
};

}  // namespace middleend::pass::analysis

#endif
