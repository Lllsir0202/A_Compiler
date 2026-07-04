#ifndef MIDDLEEND_PASS_TRANSFORM_MLIRLOOPUNROLL_MLIRLOOPUNROLL_H
#define MIDDLEEND_PASS_TRANSFORM_MLIRLOOPUNROLL_MLIRLOOPUNROLL_H

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <unordered_map>
#include "middleend/mlir/utils/clone/operationcloner.h"
#include "middleend/pass/transform/transform.h"

namespace middleend::mlir {
class Block;
class Operation;
class Region;
class Value;
}  // namespace middleend::mlir

namespace middleend::mlir::func {
class FuncDefOp;
}
namespace middleend::mlir::scf {
class WhileOp;
}

namespace middleend::pass::transform {

/**
 * @brief Full unroll of constant-trip-count while loops at the scf (MLIR) stage.
 *
 * A loop is fully unrolled when its trip count is a compile-time constant: one loop-carried value
 * is an induction variable that starts at a constant, advances by a constant step each iteration,
 * and the single-comparison loop condition bounds it against a constant. The loop is replaced by
 * that many straight-line copies of its body, threading the loop-carried values from one iteration
 * to the next, and the loop results become the values reaching the (proven) loop exit. Because the
 * body becomes straight-line code, the downstream BASEIR SCCP/GVN can then fold the now-constant
 * arithmetic.
 *
 * v1 scope (conservative, correctness-first): the condition region is a single block whose
 * condition is directly a comparison; the body contains no break/continue/return and no nested
 * while loop (nested `if` is fine). Nested loops still unroll: the innermost counted loop unrolls
 * first, which removes the inner while from the outer body and makes the outer loop eligible on a
 * later round. A cost cap bounds code growth.
 */
class MLIRLoopUnrollPass : public TransformPass {
 public:
  MLIRLoopUnrollPass(context::CompilerContext *compiler_context, PassStage stage)
      : TransformPass(compiler_context, stage) {
    if (stage != PassStage::MLIR) {
      throw std::invalid_argument("MLIRLoopUnrollPass only supports MLIR stage");
    }
  }
  ~MLIRLoopUnrollPass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "MLIRLoopUnrollPass"; }

 private:
  // A while loop found in the IR together with the block/index that holds it and its trip count.
  struct WhileSite {
    mlir::Block *block{nullptr};
    size_t index{0};
    mlir::scf::WhileOp *while_op{nullptr};
    int64_t trip_count{0};
  };

  auto RunOnFunction(mlir::func::FuncDefOp *func_op) -> bool;
  auto FindUnrollableWhile(mlir::Block *block, WhileSite *site) -> bool;
  auto CanFullyUnroll(mlir::scf::WhileOp *while_op, int64_t *trip_count) -> bool;
  auto ComputeTripCount(mlir::scf::WhileOp *while_op, int64_t *trip_count) -> bool;
  auto FullyUnroll(mlir::func::FuncDefOp *func_op, const WhileSite &site) -> void;
  // scf.condition records each loop's init values as untracked metadata (no use registered), so
  // ReplaceAllUsesWith cannot reach them. After replacing an unrolled loop's results, rewrite any
  // condition init that still points at a replaced value (e.g. a following loop that reused the
  // induction variable takes the unrolled loop's final value as its own init).
  auto PatchConditionInits(mlir::Region *region, const std::unordered_map<mlir::Value *, mlir::Value *> &subs) -> void;
  auto BodyIsUnrollable(mlir::Region *body) -> bool;
  auto OperationIsUnrollable(mlir::Operation *operation) -> bool;
  auto CountOperations(mlir::Region *region) -> size_t;

  mlir::utils::OperationCloner cloner_{compiler_context_};

  // A counted loop is unrolled only if its trip count and resulting code size stay within these
  // caps; larger loops are left for a future partial-unroller.
  static constexpr int64_t kMaxTripCount = 64;
  static constexpr int64_t kMaxUnrollCost = 512;
  // Safety bound on unroll rounds per function (each round removes one loop; nested loops need a
  // few rounds). Only guards against unexpected non-convergence.
  static constexpr size_t kMaxRounds = 4096;
};

}  // namespace middleend::pass::transform

#endif
