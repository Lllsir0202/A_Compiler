#ifndef MIDDLEEND_PASS_TRANSFORM_MLIRLOOPUNROLL_MLIRLOOPPARTIALUNROLL_H
#define MIDDLEEND_PASS_TRANSFORM_MLIRLOOPUNROLL_MLIRLOOPPARTIALUNROLL_H

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>
#include "middleend/mlir/utils/clone/operationcloner.h"
#include "middleend/pass/transform/mlirloopunroll/inductionvariable.h"
#include "middleend/pass/transform/transform.h"

namespace middleend::mlir {
class Block;
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
 * @brief Runtime partial unroll of counted while loops at the scf (MLIR) stage.
 *
 * For a loop whose trip count is not a compile-time constant but whose induction variable advances
 * by a constant additive step against a loop-invariant bound, the loop is split into a main loop
 * that runs the body K times per iteration and a remainder loop that finishes the leftover
 * iterations:
 *
 *   bound_main = bound - (K-1)*step        // loop-invariant, computed once in the preheader
 *   while (i OP bound_main) { body; body; ... body; }   // K copies, i advances by K*step
 *   while (i OP bound)      { body; }                    // 0..K-1 leftover iterations
 *
 * The remainder is the original loop reused unchanged except that its initial values now come from
 * the main loop's results, so downstream uses of the original loop results stay valid. This runs
 * after full unroll (which already removes small constant-trip loops); the surviving runtime and
 * large-constant loops are the partial-unroll candidates, and the wider straight-line main body is
 * what a later unroll-and-jam / SIMD step can vectorize.
 */
class MLIRLoopPartialUnrollPass : public TransformPass {
 public:
  MLIRLoopPartialUnrollPass(context::CompilerContext *compiler_context, PassStage stage)
      : TransformPass(compiler_context, stage) {
    if (stage != PassStage::MLIR) {
      throw std::invalid_argument("MLIRLoopPartialUnrollPass only supports MLIR stage");
    }
  }
  ~MLIRLoopPartialUnrollPass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "MLIRLoopPartialUnrollPass"; }

 private:
  auto RunOnFunction(mlir::func::FuncDefOp *func_op) -> bool;
  auto JamDotProductLoops(mlir::func::FuncDefOp *func_op) -> bool;
  auto TryJamDotProductLoop(mlir::Block *block, mlir::scf::WhileOp *while_op) -> bool;
  auto CollectWhileLoops(mlir::Block *block, std::vector<std::pair<mlir::Block *, mlir::scf::WhileOp *>> *loops)
      -> void;
  auto CanPartialUnroll(mlir::scf::WhileOp *while_op, loopiv::Induction *iv) -> bool;
  auto PartialUnroll(mlir::Block *block, mlir::scf::WhileOp *while_op, const loopiv::Induction &iv) -> void;
  auto MakeComparison(loopiv::CmpKind kind, mlir::Value *lhs, mlir::Value *rhs, mlir::Value *result_type_source)
      -> mlir::Operation *;

  mlir::utils::OperationCloner cloner_{compiler_context_};

  // Unroll factor and a cost cap on the widened main-loop body (K * body-op-count).
  static constexpr int64_t kUnrollFactor = 8;
  static constexpr int64_t kJamFactor = 4;
  static constexpr size_t kMaxBodyCost = 1024;
};

}  // namespace middleend::pass::transform

#endif
