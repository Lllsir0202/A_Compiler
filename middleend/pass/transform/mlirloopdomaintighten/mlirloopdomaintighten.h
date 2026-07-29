#ifndef MIDDLEEND_PASS_TRANSFORM_MLIRLOOPDOMAINTIGHTEN_MLIRLOOPDOMAINTIGHTEN_H
#define MIDDLEEND_PASS_TRANSFORM_MLIRLOOPDOMAINTIGHTEN_MLIRLOOPDOMAINTIGHTEN_H

#include <cstddef>
#include <stdexcept>
#include "middleend/pass/transform/transform.h"

namespace middleend::mlir {
class Block;
class Value;
}  // namespace middleend::mlir

namespace middleend::mlir::func {
class FuncDefOp;
}

namespace middleend::mlir::scf {
class IfOp;
class WhileOp;
}  // namespace middleend::mlir::scf

namespace middleend::pass::transform {

/**
 * @brief Shrink counted loop domains whose skipped suffix has no observable
 * effect.
 *
 * The first supported form is a unit-stride `iv < bound` loop whose leading
 * body branch performs only `continue(iv + 1)` when `cut < iv`.  The loop is
 * changed to run until `min(bound, cut + 1)`, and its externally visible final
 * induction value is repaired to the value produced by the original domain.
 */
class MLIRLoopDomainTightenPass : public TransformPass {
 public:
  MLIRLoopDomainTightenPass(context::CompilerContext *compiler_context, PassStage stage)
      : TransformPass(compiler_context, stage) {
    if (stage != PassStage::MLIR) {
      throw std::invalid_argument("MLIRLoopDomainTightenPass only supports MLIR stage");
    }
  }
  ~MLIRLoopDomainTightenPass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "MLIRLoopDomainTightenPass"; }

 private:
  struct Candidate {
    mlir::scf::IfOp *skip_if{nullptr};
    size_t body_index{0};
    mlir::Value *cut{nullptr};
    mlir::Value *init{nullptr};
    mlir::Value *bound{nullptr};
  };

  auto RunOnFunction(mlir::func::FuncDefOp *func_op) -> bool;
  auto TightenInBlock(mlir::func::FuncDefOp *func_op, mlir::Block *block) -> bool;
  auto FindEmptySuffix(mlir::scf::WhileOp *loop, Candidate *candidate) -> bool;
  auto Tighten(mlir::func::FuncDefOp *func_op, mlir::Block *parent, size_t loop_index, mlir::scf::WhileOp *loop,
               const Candidate &candidate) -> void;

  size_t loops_seen_{0};
  size_t candidates_{0};
  size_t tightened_{0};
};

}  // namespace middleend::pass::transform

#endif
