#ifndef MIDDLEEND_PASS_TRANSFORM_MLIRWIDEMODMULTIPLY_MLIRWIDEMODMULTIPLY_H
#define MIDDLEEND_PASS_TRANSFORM_MLIRWIDEMODMULTIPLY_MLIRWIDEMODMULTIPLY_H

#include <cstddef>
#include <stdexcept>
#include <vector>

#include "middleend/pass/transform/transform.h"

namespace middleend::mlir {
class Block;
}
namespace middleend::mlir::func {
class FuncCallOp;
class FuncDefOp;
}  // namespace middleend::mlir::func

namespace middleend::pass::transform {

class MLIRWideModMultiplyPass : public TransformPass {
 public:
  MLIRWideModMultiplyPass(context::CompilerContext *compiler_context, PassStage stage)
      : TransformPass(compiler_context, stage) {
    if (stage != PassStage::MLIR) {
      throw std::invalid_argument("MLIRWideModMultiplyPass only supports MLIR stage");
    }
  }
  ~MLIRWideModMultiplyPass() override = default;
  auto Run() -> void override;
  auto Name() const -> std::string override { return "MLIRWideModMultiplyPass"; }

  struct Candidate {
    mlir::func::FuncDefOp *function{nullptr};
    int modulus{0};
  };
  struct CallSite {
    mlir::func::FuncDefOp *caller{nullptr};
    mlir::Block *block{nullptr};
    mlir::func::FuncCallOp *call{nullptr};
  };

 private:
  auto FindCandidate(mlir::func::FuncDefOp *function, Candidate *candidate) -> bool;
  auto CollectCallSites(const Candidate &candidate, std::vector<CallSite> *sites) -> void;
  auto RewriteCall(const Candidate &candidate, const CallSite &site) -> void;

  size_t functions_{0};
  size_t candidates_{0};
  size_t calls_rewritten_{0};
};

}  // namespace middleend::pass::transform

#endif
