#ifndef MIDDLEEND_PASS_TRANSFORM_MLIRFUNCTIONINLINE_MLIRFUNCTIONINLINE_H
#define MIDDLEEND_PASS_TRANSFORM_MLIRFUNCTIONINLINE_MLIRFUNCTIONINLINE_H

#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "middleend/mlir/utils/clone/operationcloner.h"
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

namespace middleend::pass::transform {
class MLIRFunctionInlinePass : public TransformPass {
 public:
  MLIRFunctionInlinePass(context::CompilerContext *compiler_context, PassStage stage)
      : TransformPass(compiler_context, stage) {
    if (stage != PassStage::MLIR) {
      throw std::invalid_argument("MLIRFunctionInlinePass only supports MLIR stage");
    }
    AddDependency("MLIRReturnCanonicalizePass");
  }
  ~MLIRFunctionInlinePass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "MLIRFunctionInlinePass"; }

 private:
  struct InlineSite {
    mlir::func::FuncDefOp *caller{nullptr};
    mlir::Block *block{nullptr};
    size_t call_index{0};
    mlir::func::FuncCallOp *call_op{nullptr};
    mlir::func::FuncDefOp *callee{nullptr};
  };

  auto CollectFunctions() -> void;
  auto CollectCallGraph() -> void;
  auto CollectCallsInBlock(mlir::Block *block, std::unordered_set<mlir::func::FuncDefOp *> &callees) -> void;
  auto IsSelfRecursive(mlir::func::FuncDefOp *func_op) -> bool;
  auto FindInlineSites() -> std::vector<InlineSite>;
  auto RunInlineRound() -> bool;
  auto TryInlineInBlock(mlir::func::FuncDefOp *caller, mlir::Block *block) -> bool;
  auto IsInlineCandidate(mlir::func::FuncDefOp *caller, mlir::func::FuncDefOp *callee) -> bool;
  auto HasRecursivePath(mlir::func::FuncDefOp *from, mlir::func::FuncDefOp *target) -> bool;
  auto CountOperations(mlir::func::FuncDefOp *func_op) -> size_t;
  auto CanCloneOperation(mlir::Operation *operation) -> bool;
  auto CanCloneRegion(mlir::Region *region) -> bool;
  auto InlineCallSite(const InlineSite &site) -> bool;

  std::unordered_map<std::string, mlir::func::FuncDefOp *> function_map_;
  std::unordered_map<mlir::func::FuncDefOp *, std::unordered_set<mlir::func::FuncDefOp *>> call_graph_;
  mlir::utils::OperationCloner cloner_{compiler_context_};
  static constexpr size_t kMaxInlineOperationCount = 80;
  // Each round fully inlines every acyclic call site (RunInlineRound re-scans each
  // caller until fixpoint), so the program converges in one productive round plus
  // one confirming round. This bound is only a safety cap against unexpected
  // non-convergence; it is no longer the factor limiting how much gets inlined.
  static constexpr size_t kMaxInlineRounds = 8;
};

}  // namespace middleend::pass::transform

#endif
