#ifndef MIDDLEEND_PASS_TRANSFORM_MLIRDCE_MLIRDCE_H
#define MIDDLEEND_PASS_TRANSFORM_MLIRDCE_MLIRDCE_H

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include "middleend/pass/transform/transform.h"

namespace middleend::mlir {
class Operation;
}  // namespace middleend::mlir

namespace middleend::mlir::func {
class FuncDefOp;
}  // namespace middleend::mlir::func

namespace middleend::pass::transform {
class MLIRDeadFunctionEliminatePass : public TransformPass {
 public:
  MLIRDeadFunctionEliminatePass(context::CompilerContext *compiler_context, PassStage stage)
      : TransformPass(compiler_context, stage) {
    if (stage != PassStage::MLIR) {
      throw std::invalid_argument("MLIRDeadFunctionEliminatePass only supports MLIR stage");
    }
    AddDependency("MLIRFunctionInlinePass");
  }
  ~MLIRDeadFunctionEliminatePass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "MLIRDeadFunctionEliminatePass"; }

 private:
  auto CollectFunctions() -> void;
  auto MarkReachableFunctions() -> void;
  auto MarkReachableFrom(mlir::func::FuncDefOp *func_op) -> void;
  auto RemoveDeadFunctions() -> bool;
  auto IsRootFunction(const std::string &name) -> bool;

  std::unordered_map<std::string, mlir::func::FuncDefOp *> function_map_;
  std::unordered_set<mlir::func::FuncDefOp *> reachable_functions_;
};

}  // namespace middleend::pass::transform

#endif
