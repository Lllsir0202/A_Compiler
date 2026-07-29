#ifndef MIDDLEEND_PASS_TRANSFORM_MLIRCONDITIONALJAM_MLIRCONDITIONALJAM_H
#define MIDDLEEND_PASS_TRANSFORM_MLIRCONDITIONALJAM_MLIRCONDITIONALJAM_H

#include <cstddef>
#include <stdexcept>
#include "middleend/mlir/utils/clone/operationcloner.h"
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

namespace middleend::mlir::var {
class VarGetPtrOp;
class VarSetOp;
}  // namespace middleend::mlir::var

namespace middleend::pass::transform {

/**
 * @brief Unroll-and-jam a conditional, read-only reduction across four output
 * columns.
 *
 * Four independent reductions over the same induction domain are combined
 * into one loop carrying four accumulators. The original reduction body is
 * cloned per lane, including data-dependent `scf.if` operations; later GVN can
 * share invariant loads exposed in the common block. The column trip count
 * must be a proven multiple of four, and the output object may not be read by
 * the reduction.
 */
class MLIRConditionalJamPass : public TransformPass {
 public:
  MLIRConditionalJamPass(context::CompilerContext *compiler_context, PassStage stage)
      : TransformPass(compiler_context, stage), cloner_(compiler_context) {
    if (stage != PassStage::MLIR) {
      throw std::invalid_argument("MLIRConditionalJamPass only supports MLIR stage");
    }
  }
  ~MLIRConditionalJamPass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "MLIRConditionalJamPass"; }

 private:
  struct Candidate {
    mlir::scf::WhileOp *column_loop{nullptr};
    mlir::scf::WhileOp *reduction_loop{nullptr};
    mlir::var::VarGetPtrOp *output_pointer{nullptr};
    mlir::var::VarSetOp *output_store{nullptr};
    size_t column_index{0};
    size_t reduction_index{0};
    size_t accumulator_index{0};
    mlir::Value *column_step{nullptr};
    mlir::Value *reduction_init{nullptr};
    mlir::Value *accumulator_init{nullptr};
  };

  auto RunOnFunction(mlir::func::FuncDefOp *function) -> bool;
  auto JamInBlock(mlir::Block *block) -> bool;
  auto FindCandidate(mlir::scf::WhileOp *loop, Candidate *candidate) -> bool;
  auto Jam(const Candidate &candidate) -> void;

  mlir::utils::OperationCloner cloner_;
  size_t candidates_{0};
  size_t jammed_{0};
  static constexpr int64_t kLanes = 4;
};

}  // namespace middleend::pass::transform

#endif
