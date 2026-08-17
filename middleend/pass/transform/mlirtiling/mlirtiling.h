#ifndef MIDDLEEND_PASS_TRANSFORM_MLIRTILING_MLIRTILING_H
#define MIDDLEEND_PASS_TRANSFORM_MLIRTILING_MLIRTILING_H

#include <cstddef>
#include <stdexcept>
#include "middleend/mlir/utils/clone/operationcloner.h"
#include "middleend/pass/transform/transform.h"

namespace middleend::mlir {
class Block;
}  // namespace middleend::mlir

namespace middleend::mlir::func {
class FuncDefOp;
}

namespace middleend::mlir::scf {
class WhileOp;
}

namespace middleend::pass::transform {

/**
 * @brief Tile counted structured loops for cache data locality.
 *
 * Recognizes a canonical counted `scf.while` loop `for iv in [init, bound) step s`
 * (strictly ascending, additive step) whose body contains at least one nested
 * `scf.while`, and rewrites it into
 *
 *   ii = init
 *   while (ii < bound)                 # tile loop, step T
 *     limit = min(ii + T, bound)       # scf.if select (elided when bound % T == 0)
 *     iv = ii
 *     while (iv < limit)               # inner loop = clone of the original body
 *       <body, carried values threaded through>
 *     ii = ii + T
 *
 * Only non-innermost loops are tiled: the innermost loop of a nest is already
 * stride-1 contiguous and a candidate for unrolling, so strip-mining it only
 * adds loop overhead. Tiling the outer loops is where cache reuse lives (e.g. a
 * matmul's outer k loop: tiling it cuts C-traffic by T×).
 *
 * Legality (correctness-preserving, no per-testcase shape tests): the loop must
 * be recognized as counted with `iv < bound`, additive positive step, loop-
 * invariant init/bound, and its own body (descending into `scf.if` but not into
 * nested `scf.while`) must contain no `break`/`return` (which escape the loop and
 * would be retargeted by cloning). `scf.continue` is safe: it targets the
 * immediately-enclosing loop, which after tiling is the inner loop.
 */
class MLIRLoopTilingPass : public TransformPass {
 public:
  MLIRLoopTilingPass(context::CompilerContext *compiler_context, PassStage stage)
      : TransformPass(compiler_context, stage), cloner_(compiler_context) {
    if (stage != PassStage::MLIR) {
      throw std::invalid_argument("MLIRLoopTilingPass only supports MLIR stage");
    }
  }
  ~MLIRLoopTilingPass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "MLIRLoopTilingPass"; }

 private:
  auto TileInFunction(mlir::func::FuncDefOp *func_op) -> bool;
  auto TileInBlock(mlir::Block *block) -> bool;
  // Try to tile the counted loop at `index` in `parent`; returns true on success.
  auto TryTile(mlir::Block *parent, size_t index) -> bool;

  mlir::utils::OperationCloner cloner_;
  size_t candidates_{0};
  size_t tiled_{0};
  size_t shape_rejects_{0};
  size_t domain_rejects_{0};
  size_t escape_rejects_{0};
  size_t innermost_rejects_{0};
};

}  // namespace middleend::pass::transform

#endif
