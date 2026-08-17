#ifndef MIDDLEEND_PASS_TRANSFORM_MLIRVECTORIZE_MLIRVECTORIZE_H
#define MIDDLEEND_PASS_TRANSFORM_MLIRVECTORIZE_MLIRVECTORIZE_H

#include <stdexcept>
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
 * @brief Auto-vectorize contiguous `scf.while` loops into RVV intrinsics.
 *
 * Recognizes the canonical counted loop
 *
 *     for (i = 0; i < n; ++i) dst[i] = a[i] OP b[i];
 *
 * (a single element-wise binary store whose two loads and store are unit-stride
 * accesses through the induction variable) and rewrites it into a call to a
 * `rvv.<op>.<elem>` library intrinsic. The RISC-V backend expands that intrinsic
 * into an RVV `vsetvli` strip-mine loop plus a scalar tail; the aarch64 backend
 * (and the scalar RISC-V path when `-rvv` is off) never see the intrinsic, since
 * the pass is only added to the pipeline for `-rvv` on the RISC-V target.
 */
class MLIRVectorizePass : public TransformPass {
 public:
  MLIRVectorizePass(context::CompilerContext *compiler_context, PassStage stage)
      : TransformPass(compiler_context, stage) {
    if (stage != PassStage::MLIR) {
      throw std::invalid_argument("MLIRVectorizePass only supports MLIR stage");
    }
  }
  ~MLIRVectorizePass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "MLIRVectorizePass"; }

 private:
  auto VectorizeInFunction(mlir::func::FuncDefOp *function) -> bool;
  auto VectorizeInBlock(mlir::func::FuncDefOp *function, mlir::Block *block) -> bool;
  // Try to rewrite the loop at `index` in `parent` into an intrinsic call.
  auto TryVectorize(mlir::Block *parent, size_t index) -> bool;

  size_t candidates_{0};
  size_t vectorized_{0};
};

}  // namespace middleend::pass::transform

#endif
