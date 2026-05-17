#ifndef MIDDLEEND_PASS_TRANSFORM_CANONICALIZATION_PHI_H
#define MIDDLEEND_PASS_TRANSFORM_CANONICALIZATION_PHI_H
#include <stdexcept>
#include "middleend/pass/transform/transform.h"
namespace middleend::pass::transform {
class PhiCanonicalizationPass : public TransformPass {
 public:
  explicit PhiCanonicalizationPass(context::CompilerContext *compiler_context, PassStage stage)
      : TransformPass(compiler_context, stage) {
    if (stage != PassStage::BASEIR && stage != PassStage::LLVMIR) {
      // Only allow BASEIR or LLVM Stage
      throw std::invalid_argument("PhiCanonicalizationPass only supports BASEIR or LLVMIR stage");
    }
    AddDependency("CFGPass");
  }
  ~PhiCanonicalizationPass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "PhiCanonicalizationPass"; }

 private:
  auto CanonicalizePhiInFunction() -> void;
  auto CanonicalizePhiInLLVMFunction() -> void;
};

}  // namespace middleend::pass::transform
#endif