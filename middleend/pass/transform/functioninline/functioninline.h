// BASEIR function inlining after structured control flow has been flattened.

#ifndef MIDDLEEND_PASS_TRANSFORM_FUNCTIONINLINE_FUNCTIONINLINE_H
#define MIDDLEEND_PASS_TRANSFORM_FUNCTIONINLINE_FUNCTIONINLINE_H

#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include "middleend/pass/transform/transform.h"

namespace middleend::mlir {
class Block;
class Operation;
class Value;
}  // namespace middleend::mlir

namespace middleend::mlir::base {
class FuncCallOp;
class FuncDefOp;
}  // namespace middleend::mlir::base

namespace middleend::pass::transform {
class FunctionInlinePass : public TransformPass {
 public:
  FunctionInlinePass(context::CompilerContext *compiler_context, PassStage stage)
      : TransformPass(compiler_context, stage) {
    if (stage != PassStage::BASEIR) {
      throw std::invalid_argument("FunctionInlinePass only supports BASEIR stage");
    }
    AddDependency("CFGPass");
    AddDependency("UnifyReturnPass");
  }
  ~FunctionInlinePass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "FunctionInlinePass"; }

 private:
  struct InlineSite {
    mlir::base::FuncDefOp *caller{nullptr};
    mlir::Block *call_block{nullptr};
    size_t call_index{0};
    mlir::base::FuncCallOp *call_op{nullptr};
    mlir::base::FuncDefOp *callee{nullptr};
  };

  auto CollectFunctions() -> void;
  auto CollectCallGraph() -> void;
  auto IsInlineCandidate(mlir::base::FuncDefOp *caller, mlir::base::FuncDefOp *callee) -> bool;
  auto HasRecursivePath(mlir::base::FuncDefOp *from, mlir::base::FuncDefOp *target) -> bool;
  auto CountOperations(mlir::base::FuncDefOp *func_op) -> size_t;
  auto FindInlineSites() -> std::vector<InlineSite>;
  auto InlineCallSite(const InlineSite &site) -> bool;
  auto CloneOperation(mlir::Operation *operation) -> mlir::Operation *;
  auto CloneValue(mlir::Value *original) -> mlir::Value *;
  auto RemapValue(mlir::Value *value) -> mlir::Value *;
  auto RemapBlock(mlir::Block *block) -> mlir::Block *;
  auto CreateInlineBlock(const std::string &prefix) -> mlir::Block *;
  auto RebuildUses(mlir::base::FuncDefOp *func_op) -> void;
  auto RebuildOperationUses(mlir::Operation *operation) -> void;
  auto RebuildPhiUses(mlir::Operation *operation) -> void;
  auto HasUnifiedReturn(mlir::base::FuncDefOp *callee) -> bool;

  std::unordered_map<std::string, mlir::base::FuncDefOp *> function_map_;
  std::unordered_map<mlir::base::FuncDefOp *, std::unordered_set<mlir::base::FuncDefOp *>> call_graph_;
  std::unordered_map<mlir::Value *, mlir::Value *> value_map_;
  std::unordered_map<mlir::Block *, mlir::Block *> block_map_;
  size_t inline_block_id_counter_{0};
  static constexpr size_t kMaxInlineOperationCount = 200;
  static constexpr size_t kMaxInlineSites = 256;
};

}  // namespace middleend::pass::transform

#endif
