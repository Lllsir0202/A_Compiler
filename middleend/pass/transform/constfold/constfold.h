#ifndef MIDDLEEND_PASS_TRANSFORM_CONSTFOLD_CONSTFOLD_H
#define MIDDLEEND_PASS_TRANSFORM_CONSTFOLD_CONSTFOLD_H

#include <stdexcept>
#include "middleend/llvm/utils/struct/function.h"
#include "middleend/pass/transform/transform.h"

namespace middleend::llvm::inst {
class Instruction;
}

namespace middleend::llvm::utils {
class Operand;
}

namespace middleend::pass::transform {
class ConstFoldPass : public TransformPass {
 public:
  explicit ConstFoldPass(context::CompilerContext *compiler_context, PassStage stage)
      : TransformPass(compiler_context, stage) {
    if (stage != PassStage::LLVMIR) {
      // Only allow LLVM Stage
      throw std::invalid_argument("ConstFoldPass only supports LLVMIR stage");
    }
    // No dependencies
  }
  ~ConstFoldPass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "ConstFoldPass"; }

 private:
  auto FoldConstantsInFunction(llvm::utils::Function *function) -> void;
  auto TryComputeConstantResult(llvm::inst::Instruction *inst) -> llvm::utils::Operand *;
  auto ComputeArithmResult(llvm::inst::Instruction *inst) -> llvm::utils::Operand *;
  auto ComputeCmpResult(llvm::inst::Instruction *inst) -> llvm::utils::Operand *;
  auto ComputeCastResult(llvm::inst::Instruction *inst) -> llvm::utils::Operand *;
};
}  // namespace middleend::pass::transform

#endif
