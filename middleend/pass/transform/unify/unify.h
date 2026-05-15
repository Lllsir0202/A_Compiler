#ifndef MIDDLEEND_PASS_TRANSFORM_UNIFY_UNIFY_H
#define MIDDLEEND_PASS_TRANSFORM_UNIFY_UNIFY_H

#include <stdexcept>
#include "middleend/pass/transform/transform.h"
namespace middleend::pass::transform {
/**
 * @brief Unify the return to make a function have only one return operation
 * @note Only call in BASEIR stage
 */
class UnifyReturnPass : public TransformPass {
 public:
  UnifyReturnPass(context::CompilerContext *compiler_context, PassStage stage)
      : TransformPass(compiler_context, stage) {
    if (stage != PassStage::BASEIR) {
      // Only allow BASEIR Stage
      throw std::invalid_argument("UnifyReturnPass only supports BASEIR stage");
    }
    // Depend on CFGPass,
    // we get cfg information in this pass to unify the return operation
    AddDependency("CFGPass");
  }
  ~UnifyReturnPass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "UnifyReturnPass"; }

 private:
  auto UnifyReturnInFunction() -> void;
};

}  // namespace middleend::pass::transform

#endif