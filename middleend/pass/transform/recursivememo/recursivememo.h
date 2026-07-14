#ifndef MIDDLEEND_PASS_TRANSFORM_RECURSIVEMEMO_RECURSIVEMEMO_H
#define MIDDLEEND_PASS_TRANSFORM_RECURSIVEMEMO_RECURSIVEMEMO_H

#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>
#include "middleend/pass/transform/transform.h"

namespace middleend::mlir {
class Block;
class Operation;
class Value;
}  // namespace middleend::mlir

namespace middleend::mlir::base {
class FuncDefOp;
}  // namespace middleend::mlir::base

namespace middleend::mlir::var {
class GlobalVarDefOp;
}  // namespace middleend::mlir::var

namespace middleend::pass::transform {

/**
 * Memoize safe two-integer direct recursion behind a bounded wrapper.
 *
 * The transformation is deliberately narrow: the recursive body may only call
 * itself, may not write memory, must have multiple recursive call sites, and
 * must have exactly one straight-line top-level call from main.  These guards
 * make a zero-initialized process-lifetime cache sound even when the function
 * reads global arrays initialized by main.
 */
class RecursiveMemoizationPass : public TransformPass {
 public:
  RecursiveMemoizationPass(context::CompilerContext *compiler_context, PassStage stage)
      : TransformPass(compiler_context, stage) {
    if (stage != PassStage::BASEIR) {
      throw std::invalid_argument("RecursiveMemoizationPass only supports BASEIR stage");
    }
    AddDependency("UnifyReturnPass");
  }
  ~RecursiveMemoizationPass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "RecursiveMemoizationPass"; }

 private:
  struct Candidate {
    mlir::base::FuncDefOp *function{nullptr};
    size_t recursive_calls{0};
  };

  auto FindCandidates() -> std::vector<Candidate>;
  auto IsSafeRecursiveBody(mlir::base::FuncDefOp *function, size_t *recursive_calls) -> bool;
  auto HasSingleStraightLineMainCall(mlir::base::FuncDefOp *function) -> bool;
  auto Rewrite(const Candidate &candidate, const std::unordered_set<std::string> &used_names) -> bool;
  auto UniqueName(const std::string &base, const std::unordered_set<std::string> &used_names) const -> std::string;
  auto MakeGlobalArray(const std::string &name) -> mlir::var::GlobalVarDefOp *;
  auto MakeWrapper(const std::string &public_name, const std::string &impl_name,
                   mlir::base::FuncDefOp *original, mlir::Value *cache_values,
                   mlir::Value *cache_valid) -> mlir::base::FuncDefOp *;
  auto NameBlock(mlir::Block *block, const std::string &name) -> void;
};

}  // namespace middleend::pass::transform

#endif
