#ifndef MIDDLEEND_PASS_ANALYSIS_MLIRMEMORYEFFECTS_MLIRMEMORYEFFECTS_H
#define MIDDLEEND_PASS_ANALYSIS_MLIRMEMORYEFFECTS_MLIRMEMORYEFFECTS_H

#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include "middleend/pass/analysis/analysis.h"
#include "middleend/pass/analysis/sideeffect/sideeffect.h"

namespace middleend::mlir {
class Operation;
class Value;
}  // namespace middleend::mlir

namespace middleend::mlir::func {
class FuncCallOp;
class FuncDefOp;
}  // namespace middleend::mlir::func

namespace middleend::mlir::scf {
class WhileOp;
}  // namespace middleend::mlir::scf

namespace middleend::pass::analysis {

/**
 * @brief Structured-MLIR memory-effect analysis used before CFG lowering.
 *
 * The analysis computes closed-world, fixed-point function Mod/Ref summaries.
 * Object-sensitive loop queries are evaluated against the current structured
 * IR, so transformations do not leave stale per-loop state behind.
 */
class MLIRMemoryEffectAnalysis : public AnalysisPass {
 public:
  enum class RootKind : uint8_t { kLocal, kGlobal, kParameter, kUnknown };

  struct PointerRoot {
    RootKind kind{RootKind::kUnknown};
    mlir::Value *value{nullptr};
  };

  MLIRMemoryEffectAnalysis(context::CompilerContext *compiler_context, PassStage stage)
      : AnalysisPass(compiler_context, stage) {
    if (stage != PassStage::MLIR) {
      throw std::invalid_argument("MLIRMemoryEffectAnalysis only supports MLIR stage");
    }
  }
  ~MLIRMemoryEffectAnalysis() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "MLIRMemoryEffectAnalysis"; }

  auto GetSummary(const std::string &name) const -> const SideEffectSummary &;
  auto IsReadNoneAndIOClean(mlir::func::FuncCallOp *call) const -> bool;
  auto CallMayWrite(mlir::func::FuncCallOp *call, mlir::Value *pointer) const -> bool;
  auto LoopMayWrite(mlir::scf::WhileOp *loop, mlir::Value *pointer) const -> bool;
  auto IsSafeToSpeculateLoad(mlir::Value *pointer) const -> bool;

  static auto GetPointerRoot(mlir::Value *pointer) -> PointerRoot;
  static auto RootsAreDistinct(const PointerRoot &lhs, const PointerRoot &rhs) -> bool;
  static auto MayAlias(mlir::Value *lhs, mlir::Value *rhs) -> bool;

 private:
  auto AnalyzeFunction(mlir::func::FuncDefOp *function) const -> SideEffectSummary;
  auto AnalyzeOperation(mlir::Operation *operation, const std::vector<mlir::Value *> &parameters,
                        SideEffectSummary *summary) const -> void;
  auto OperationMayWrite(mlir::Operation *operation, mlir::Value *pointer) const -> bool;

  std::unordered_map<std::string, mlir::func::FuncDefOp *> functions_;
  std::unordered_map<std::string, SideEffectSummary> summaries_;
};

}  // namespace middleend::pass::analysis

#endif
