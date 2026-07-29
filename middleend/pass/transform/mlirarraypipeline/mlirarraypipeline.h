#ifndef MIDDLEEND_PASS_TRANSFORM_MLIRARRAYPIPELINE_MLIRARRAYPIPELINE_H
#define MIDDLEEND_PASS_TRANSFORM_MLIRARRAYPIPELINE_MLIRARRAYPIPELINE_H

#include <cstddef>
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
 * @brief Fuse algebraically reducible array pipelines.
 *
 * The first pattern is a square-matrix pipeline that broadcasts one scalar
 * value per row, performs an in-place negated transpose, then sums the whole
 * matrix. Its result is exactly the initial sum minus the sum of the broadcast
 * row values, so materializing and rereading the matrix is unnecessary.
 */
class MLIRArrayPipelinePass : public TransformPass {
 public:
  MLIRArrayPipelinePass(context::CompilerContext *compiler_context, PassStage stage)
      : TransformPass(compiler_context, stage) {
    if (stage != PassStage::MLIR) {
      throw std::invalid_argument("MLIRArrayPipelinePass only supports MLIR stage");
    }
  }
  ~MLIRArrayPipelinePass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "MLIRArrayPipelinePass"; }

 private:
  struct Candidate {
    mlir::scf::WhileOp *row_loop{nullptr};
    mlir::scf::WhileOp *producer_loop{nullptr};
    mlir::scf::WhileOp *broadcast_loop{nullptr};
    mlir::scf::WhileOp *transpose_loop{nullptr};
    mlir::scf::WhileOp *sum_loop{nullptr};
    mlir::Value *row_value{nullptr};
    mlir::Value *sum_init{nullptr};
    size_t transpose_index{0};
    size_t sum_index{0};
  };

  auto RunOnFunction(mlir::func::FuncDefOp *function) -> bool;
  auto FuseInBlock(mlir::func::FuncDefOp *function, mlir::Block *block) -> bool;
  auto FindCandidate(mlir::Block *parent, size_t row_index, Candidate *candidate) -> bool;
  auto Fuse(mlir::func::FuncDefOp *function, mlir::Block *parent, const Candidate &candidate) -> void;

  size_t candidates_{0};
  size_t fused_{0};
};

}  // namespace middleend::pass::transform

#endif
