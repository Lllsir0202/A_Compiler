#ifndef MIDDLEEND_PASS_TRANSFORM_MLIRMATRIXROWSUMMARY_MLIRMATRIXROWSUMMARY_H
#define MIDDLEEND_PASS_TRANSFORM_MLIRMATRIXROWSUMMARY_MLIRMATRIXROWSUMMARY_H

#include <cstddef>
#include <stdexcept>
#include <vector>

#include "middleend/pass/transform/transform.h"

namespace middleend::mlir {
class Block;
class Operation;
class Value;
}  // namespace middleend::mlir
namespace middleend::mlir::func {
class FuncCallOp;
class FuncDefOp;
}  // namespace middleend::mlir::func
namespace middleend::mlir::scf {
class WhileOp;
}
namespace middleend::mlir::var {
class VarGetPtrOp;
}

namespace middleend::pass::transform {

/**
 * Replace a whole-program matrix pipeline with exact row summaries when the
 * only final observation is an additive checksum.
 *
 * For C[i][j] = C[i][j] * A[i][k] + B[k][j], summing over j commutes with the
 * update in the i32 modular ring.  The pass keeps each row summary in column
 * zero, while preserving the full A matrix and the original conditional skip.
 */
class MLIRMatrixRowSummaryPass : public TransformPass {
 public:
  MLIRMatrixRowSummaryPass(context::CompilerContext *compiler_context, PassStage stage)
      : TransformPass(compiler_context, stage) {
    if (stage != PassStage::MLIR) {
      throw std::invalid_argument("MLIRMatrixRowSummaryPass only supports MLIR stage");
    }
  }
  ~MLIRMatrixRowSummaryPass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "MLIRMatrixRowSummaryPass"; }

 private:
  struct Kernel {
    mlir::func::FuncDefOp *function{nullptr};
    mlir::Value *bound{nullptr};
    mlir::Value *one{nullptr};
    mlir::scf::WhileOp *zero_column_loop{nullptr};
    mlir::scf::WhileOp *update_column_loop{nullptr};
  };

  struct InputLoop {
    mlir::scf::WhileOp *row_loop{nullptr};
    mlir::scf::WhileOp *column_loop{nullptr};
    mlir::func::FuncCallOp *input_call{nullptr};
    mlir::var::VarGetPtrOp *store_pointer{nullptr};
  };

  struct Candidate {
    Kernel kernel;
    mlir::func::FuncDefOp *caller{nullptr};
    mlir::Value *summary_root{nullptr};
    mlir::Value *scratch_root{nullptr};
    mlir::Value *bound{nullptr};
    InputLoop input;
    mlir::scf::WhileOp *checksum_column_loop{nullptr};
  };

  auto FindKernel(mlir::func::FuncDefOp *function, Kernel *kernel) -> bool;
  auto FindCandidate(const Kernel &kernel, Candidate *candidate) -> bool;
  auto Rewrite(const Candidate &candidate) -> void;

  size_t kernels_{0};
  size_t candidates_{0};
  size_t summarized_{0};
};

}  // namespace middleend::pass::transform

#endif
