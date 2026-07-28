#ifndef MIDDLEEND_PASS_TRANSFORM_MLIRTAILRECURSION_MLIRTAILRECURSION_H
#define MIDDLEEND_PASS_TRANSFORM_MLIRTAILRECURSION_MLIRTAILRECURSION_H

#include <stdexcept>
#include <unordered_map>
#include <vector>
#include "middleend/pass/transform/transform.h"

namespace middleend::mlir {
class Block;
class Operation;
class Region;
class Value;
}  // namespace middleend::mlir

namespace middleend::mlir::func {
class FuncCallOp;
class FuncDefOp;
class ReturnOp;
}  // namespace middleend::mlir::func

namespace middleend::mlir::scf {
class IfOp;
class YieldOp;
}  // namespace middleend::mlir::scf

namespace middleend::pass::transform {
class MLIRTailRecursionEliminatePass : public TransformPass {
 public:
  MLIRTailRecursionEliminatePass(context::CompilerContext *compiler_context, PassStage stage)
      : TransformPass(compiler_context, stage) {
    if (stage != PassStage::MLIR) {
      throw std::invalid_argument("MLIRTailRecursionEliminatePass only supports MLIR stage");
    }
    AddDependency("MLIRReturnCanonicalizePass");
  }
  ~MLIRTailRecursionEliminatePass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "MLIRTailRecursionEliminatePass"; }

 private:
  struct TailRecursionPattern {
    mlir::scf::IfOp *if_op{nullptr};
    mlir::func::ReturnOp *return_op{nullptr};
    mlir::func::FuncCallOp *call_op{nullptr};
    mlir::scf::YieldOp *then_yield{nullptr};
    mlir::scf::YieldOp *else_yield{nullptr};
    size_t if_index{0};
  };

  auto RunOnFunction(mlir::func::FuncDefOp *func_op) -> bool;
  auto MatchTailRecursion(mlir::func::FuncDefOp *func_op, TailRecursionPattern *pattern) -> bool;
  auto RewriteTailRecursion(mlir::func::FuncDefOp *func_op, const TailRecursionPattern &pattern) -> bool;
  auto RewriteMultiExitTailRecursion(mlir::func::FuncDefOp *func_op) -> bool;
  // Both walkers take a start index so that the operations after an scf.if can be treated as that
  // if's fall-through continuation: `if (c) return f(..);` followed by more code is the natural way
  // to write guarded tail recursion, and its else region is empty rather than terminal.
  auto IsTerminalTailBlock(mlir::Block *block, mlir::func::FuncDefOp *func_op, bool *saw_tail,
                           size_t start = 0) -> bool;
  auto BuildTailState(mlir::Block *source, mlir::Block *destination, mlir::func::FuncDefOp *func_op,
                      const std::vector<mlir::Value *> &current_args, mlir::Value *current_result,
                      size_t start = 0) -> std::vector<mlir::Value *>;
  // True when the region is a single block whose only operation is an operand-less scf.yield, i.e. an
  // `if` with no `else`: control leaves it by falling through to the enclosing block.
  static auto IsFallThroughRegion(mlir::Region *region) -> bool;
  auto CloneOperation(mlir::Operation *operation) -> mlir::Operation *;
  auto CloneValue(mlir::Value *value) -> mlir::Value *;
  auto RemapValue(mlir::Value *value) -> mlir::Value *;
  auto CanCloneOperation(mlir::Operation *operation) -> bool;
  auto RebuildUses(mlir::func::FuncDefOp *func_op) -> void;
  auto RebuildUsesInOperation(mlir::Operation *operation) -> void;

  std::unordered_map<mlir::Value *, mlir::Value *> value_map_;
  mlir::Value *tail_continue_{nullptr};
  mlir::Value *tail_stop_{nullptr};
};

}  // namespace middleend::pass::transform

#endif
