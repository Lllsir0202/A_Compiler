#ifndef MIDDLEEND_PASS_TRANSFORM_REASSOCIATE_REASSOCIATE_H
#define MIDDLEEND_PASS_TRANSFORM_REASSOCIATE_REASSOCIATE_H

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "middleend/pass/transform/transform.h"

namespace middleend::mlir {
class Block;
class Operation;
class Value;
}  // namespace middleend::mlir

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
  auto Linearize(mlir::Operation *operation,
                 const std::unordered_set<mlir::Operation *> &seen,
                 std::vector<mlir::Operation *> *replacement) -> bool;

  std::unordered_map<mlir::Value *, size_t> use_count_;
  size_t linear_candidates_{0};
  size_t linear_simplified_{0};
};

}  // namespace middleend::pass::transform

#endif
