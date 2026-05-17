#ifndef MIDDLEEND_PASS_TRANSFORM_SIMPLIFY_SIMPLIFY_H
#define MIDDLEEND_PASS_TRANSFORM_SIMPLIFY_SIMPLIFY_H

#include <stdexcept>
#include "middleend/llvm/utils/struct/function.h"
#include "middleend/mlir/dialect/base/funcdef.h"
#include "middleend/mlir/utils/struct/block.h"
#include "middleend/pass/transform/transform.h"
namespace middleend::pass::transform {
// This pass should invalidate the CFG and all the analyses
// relying on CFG,
// since we will modify the CFG structure in this pass.
class SimplifyPass : public TransformPass {
 public:
  SimplifyPass(context::CompilerContext *compiler_context, PassStage stage) : TransformPass(compiler_context, stage) {
    if (stage != PassStage::BASEIR && stage != PassStage::LLVMIR) {
      // Only allow BASEIR or LLVM Stage
      throw std::invalid_argument("SimplifyPass only supports BASEIR or LLVMIR stage");
    }
  }
  ~SimplifyPass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "SimplifyPass"; }

 private:
  // Simplify the cfg and eliminate unreachable blocks
  auto SimplifyCFG() -> void;
  auto SimplifyCFGBaseIR() -> void;
  auto SimplifyCFGLLVMIR() -> void;

  auto SimplifyCFGInFunction(mlir::base::FuncDefOp *func_op) -> void;
  auto SimplifyCFGInLLVMFunction(llvm::utils::Function *function) -> void;

  std::vector<mlir::Block *> to_delete_blocks_;
};

}  // namespace middleend::pass::transform

#endif