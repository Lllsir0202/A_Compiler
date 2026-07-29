#ifndef MIDDLEEND_PASS_TRANSFORM_MLIRROWCHECKSUM_MLIRROWCHECKSUM_H
#define MIDDLEEND_PASS_TRANSFORM_MLIRROWCHECKSUM_MLIRROWCHECKSUM_H

#include <cstddef>
#include <stdexcept>
#include <unordered_set>
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
class WhileOp;
}
namespace middleend::mlir::var {
class VarGetValOp;
}

namespace middleend::pass::transform {

class MLIRRowChecksumPass : public TransformPass {
 public:
  MLIRRowChecksumPass(context::CompilerContext *compiler_context, PassStage stage)
      : TransformPass(compiler_context, stage), cloner_(compiler_context) {
    if (stage != PassStage::MLIR) {
      throw std::invalid_argument("MLIRRowChecksumPass only supports MLIR stage");
    }
  }
  ~MLIRRowChecksumPass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "MLIRRowChecksumPass"; }

 private:
  struct Candidate {
    mlir::func::FuncDefOp *function{nullptr};
    mlir::Block *parent{nullptr};
    mlir::scf::WhileOp *row_loop{nullptr};
    mlir::scf::WhileOp *sum_loop{nullptr};
    mlir::scf::WhileOp *update_loop{nullptr};
    mlir::scf::WhileOp *checksum_loop{nullptr};
    mlir::var::VarGetValOp *bound_load{nullptr};
    mlir::Value *root{nullptr};
    size_t row_index{0};
    size_t checksum_index{0};
    size_t sum_accumulator{0};
    size_t checksum_accumulator{0};
  };

  auto CollectCalledFunctions(mlir::Block *block,
                              std::unordered_set<std::string> *called) -> void;
  auto FindInBlock(mlir::func::FuncDefOp *function, mlir::Block *block,
                   const std::unordered_set<std::string> &called) -> bool;
  auto FindCandidate(mlir::func::FuncDefOp *function, mlir::Block *parent,
                     size_t row_index, Candidate *candidate) -> bool;
  auto Fuse(const Candidate &candidate) -> void;

  mlir::utils::OperationCloner cloner_;
  size_t candidates_{0};
  size_t fused_{0};
};

}  // namespace middleend::pass::transform

#endif
