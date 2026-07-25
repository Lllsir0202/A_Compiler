#ifndef MIDDLEEND_PASS_ANALYSIS_ALIAS_ALIAS_H
#define MIDDLEEND_PASS_ANALYSIS_ALIAS_ALIAS_H

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include "middleend/pass/analysis/analysis.h"

namespace middleend::mlir {
class Operation;
class Value;
}  // namespace middleend::mlir

namespace middleend::pass::analysis {
class MLIRFunctionCFG;
class FunctionSideEffectPass;
class SCEVPass;

enum class AliasResult : uint8_t { NoAlias, MayAlias, MustAlias };

/**
 * @brief Points-to based alias analysis for the BASEIR memory model.
 *
 * Every pointer traces back, through the `var.getptr` address-computation chain, to a base
 * memory object: a local (`var.def`), a global (`var.globaldef`), a function parameter
 * pointer, or an untrackable "unknown". Two pointers cannot alias when their base objects
 * are provably different storage, e.g. two distinct locals, a local and a global, or a
 * local whose address never escapes versus anything else (nothing external can name it).
 *
 * SysY makes this precise and cheap: objects are named `var.def`/`var.globaldef` results,
 * the only ways an address escapes are being passed to a call, returned, or stored into
 * memory, and O1 inlining resolves many parameter pointers to concrete caller objects.
 */
class AliasAnalysisPass : public AnalysisPass {
 public:
  explicit AliasAnalysisPass(context::CompilerContext *compiler_context, PassStage stage, bool require_scev = true)
      : AnalysisPass(compiler_context, stage) {
    AddDependency("CFGPass");
    if (require_scev) {
      AddDependency("SCEVPass");
    }
  }
  ~AliasAnalysisPass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "AliasAnalysisPass"; }
  auto IsAnalysisPass() -> bool override { return true; }

  enum class ObjectKind : uint8_t { Local, Global, Param, Unknown };

  /**
   * @brief The base storage a pointer is derived from. `root` is the defining value of the
   * object (a `var.def` / `var.globaldef` result, or a parameter value); it is nullptr for
   * the unknown object.
   */
  struct MemoryObject {
    ObjectKind kind{ObjectKind::Unknown};
    mlir::Value *root{nullptr};
    // A phi can prove the underlying object while leaving the concrete
    // address path-dependent. Such a fact is valid for NoAlias, not MustAlias.
    bool via_phi{false};
  };

  // Base object a pointer is derived from (follows getptr/copy chains and pointer
  // phis, memoized).
  auto GetBaseObject(mlir::Value *pointer) -> MemoryObject;
  // Core resolver used by GetBaseObject; `visiting` breaks pointer-phi cycles
  // (a strength-reduced pointer induction variable is a phi of itself).
  auto ResolveBaseObject(mlir::Value *pointer, std::unordered_set<mlir::Value *> &visiting) -> MemoryObject;
  // Aliasing relationship between two pointers.
  auto Alias(mlir::Value *lhs, mlir::Value *rhs) -> AliasResult;
  // Whether the callee could name `object` at all during this call. A local whose address never
  // escapes is reachable from a callee only by being handed to *this* call as an argument;
  // "does not escape" alone does not make it invisible to a call that receives it.
  auto CallCanNameObject(mlir::Operation *call, const MemoryObject &object) -> bool;
  // Whether the call may modify or reference the object behind `pointer`.
  auto CallModRef(mlir::Operation *call, mlir::Value *pointer) -> bool;
  auto CallModRefPrecise(mlir::Operation *call, mlir::Value *pointer) -> bool;
  auto GetSideEffect() -> FunctionSideEffectPass * { return side_effect_; }
  // Whether an object may be reached by external code (calls / other pointers).
  auto IsEscaped(const MemoryObject &object) -> bool;

 private:
  auto CollectParameters() -> void;
  auto ComputeEscape() -> void;
  // Closed-world direct-call proof: a pair of pointer parameters is no-alias
  // when every call site supplies storage that is already known distinct.
  auto CollectInterproceduralNoAliasFacts() -> void;
  auto MarkEscapeIfLocal(mlir::Value *pointer) -> void;
  auto SameObjectAlias(mlir::Value *lhs, mlir::Value *rhs) -> AliasResult;
  auto DefinitelyDistinct(const MemoryObject &lhs, const MemoryObject &rhs) -> bool;
  auto AreProvenNoAliasParameters(mlir::Value *lhs, mlir::Value *rhs) const -> bool;
  auto GetConstantIndex(mlir::Value *value, int *out) -> bool;
  // True when SCEV can prove the two index values differ for every iteration.
  auto SCEVNoAlias(mlir::Value *lhs_idx, mlir::Value *rhs_idx) -> bool;

  std::unordered_map<mlir::Value *, MemoryObject> base_cache_;
  std::unordered_set<mlir::Value *> param_values_;
  std::unordered_set<mlir::Value *> escaped_locals_;
  std::unordered_map<mlir::Value *, std::unordered_set<mlir::Value *>> noalias_parameters_;
  FunctionSideEffectPass *side_effect_{nullptr};
  SCEVPass *scev_{nullptr};
};

// Basic points-to/mod-ref analysis used to expose memory-carried recurrences
// before SCEV exists.  The full AliasAnalysisPass runs after promotion and
// SCEV, adding recurrence-aware index disambiguation for later consumers.
class PrePromotionAliasAnalysisPass : public AliasAnalysisPass {
 public:
  explicit PrePromotionAliasAnalysisPass(context::CompilerContext *compiler_context, PassStage stage)
      : AliasAnalysisPass(compiler_context, stage, false) {
    AddDependency("FunctionSideEffectPass");
  }
  ~PrePromotionAliasAnalysisPass() override = default;

  auto Name() const -> std::string override { return "PrePromotionAliasAnalysisPass"; }
};

}  // namespace middleend::pass::analysis

#endif
