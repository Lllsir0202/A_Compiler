#ifndef MIDDLEEND_PASS_TRANSFORM_REASSOCIATE_REASSOCIATE_H
#define MIDDLEEND_PASS_TRANSFORM_REASSOCIATE_REASSOCIATE_H

#include <string>
#include <unordered_map>
#include "middleend/pass/transform/transform.h"

namespace middleend::mlir {
class Block;
class Value;
}

namespace middleend::pass::transform {

/** Canonicalize integer add/multiply trees to expose local common subexpressions. */
class ReassociatePass : public TransformPass {
 public:
  ReassociatePass(context::CompilerContext *compiler_context, PassStage stage)
      : TransformPass(compiler_context, stage) {
    if (stage != PassStage::BASEIR) {
      throw std::invalid_argument("ReassociatePass only supports BASEIR stage");
    }
    AddDependency("CFGPass");
  }
  ~ReassociatePass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "ReassociatePass"; }

 private:
  auto RunOnBlock(mlir::Block *block) -> bool;

  std::unordered_map<mlir::Value *, size_t> use_count_;
};

}  // namespace middleend::pass::transform

#endif
