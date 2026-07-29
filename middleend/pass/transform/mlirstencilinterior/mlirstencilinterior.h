#ifndef MIDDLEEND_PASS_TRANSFORM_MLIRSTENCILINTERIOR_MLIRSTENCILINTERIOR_H
#define MIDDLEEND_PASS_TRANSFORM_MLIRSTENCILINTERIOR_MLIRSTENCILINTERIOR_H

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>
#include "middleend/mlir/utils/clone/operationcloner.h"
#include "middleend/pass/transform/transform.h"

namespace middleend::mlir {
class Block;
class Value;
}

namespace middleend::mlir::func {
class FuncDefOp;
}

namespace middleend::mlir::scf {
class IfOp;
class WhileOp;
}

namespace middleend::mlir::var {
class VarGetValOp;
class VarSetOp;
}

namespace middleend::pass::transform {

/**
 * @brief Version a square constant-radius stencil into interior and border
 * paths.
 *
 * The border path is kept byte-for-byte as structured IR.  The interior path
 * clones the reduction and specializes its proven
 * `0 <= row+kr-pad < bound && 0 <= col+kc-pad < bound` guard to true.  This
 * exposes a branch-free constant kernel to the existing full unroller.
 */
class MLIRStencilInteriorPass : public TransformPass {
 public:
  MLIRStencilInteriorPass(context::CompilerContext *compiler_context, PassStage stage)
      : TransformPass(compiler_context, stage), cloner_(compiler_context) {
    if (stage != PassStage::MLIR) {
      throw std::invalid_argument("MLIRStencilInteriorPass only supports MLIR stage");
    }
  }
  ~MLIRStencilInteriorPass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "MLIRStencilInteriorPass"; }

 private:
  struct Candidate {
    mlir::func::FuncDefOp *function{nullptr};
    mlir::Block *column_body{nullptr};
    mlir::scf::WhileOp *row_loop{nullptr};
    mlir::scf::WhileOp *column_loop{nullptr};
    mlir::scf::WhileOp *row_kernel_loop{nullptr};
    mlir::scf::WhileOp *column_kernel_loop{nullptr};
    mlir::scf::IfOp *guarded_update{nullptr};
    std::vector<mlir::scf::IfOp *> logical_ands;
    mlir::var::VarGetValOp *bound_load{nullptr};
    mlir::var::VarSetOp *output_store{nullptr};
    mlir::Value *row{nullptr};
    mlir::Value *column{nullptr};
    size_t row_kernel_accumulator{0};
    int32_t pad{0};
    int32_t maximum_offset{0};
  };

  auto CollectCandidates(mlir::func::FuncDefOp *function, mlir::Block *block,
                         mlir::scf::WhileOp *parent, std::vector<Candidate> *candidates) -> void;
  auto FindCandidate(mlir::func::FuncDefOp *function, mlir::scf::WhileOp *row_loop,
                     mlir::scf::WhileOp *column_loop, Candidate *candidate) -> bool;
  auto Specialize(const Candidate &candidate) -> void;

  mlir::utils::OperationCloner cloner_;
  size_t candidates_{0};
  size_t specialized_{0};
};

}  // namespace middleend::pass::transform

#endif
