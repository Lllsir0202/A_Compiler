#ifndef MIDDLEEND_PASS_ANALYSIS_SIDEEFFECT_SIDEEFFECT_H
#define MIDDLEEND_PASS_ANALYSIS_SIDEEFFECT_SIDEEFFECT_H

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "middleend/pass/analysis/analysis.h"

namespace middleend::mlir {
class Value;
class Operation;
class Block;
}  // namespace middleend::mlir

namespace middleend::pass::analysis {
class MLIRFunctionCFG;
class CFGPass;

// Per-callee side-effect summary used by AliasAnalysis::CallModRef and by every
// pass that currently treats any call as clobber-all.
struct SideEffectSummary {
  // The callee may touch every global (NoInfo / MayAll — a safe overapproximation).
  bool reads_all_globals{false};
  bool writes_all_globals{false};

  // When not reads_all_globals / writes_all_globals, these are the specific
  // global-definition values that are read or written.  (A call can both read
  // and write the same global.)
  std::unordered_set<mlir::Value *> read_globals;
  std::unordered_set<mlir::Value *> written_globals;

  // IO: calling getint/putint etc. is observable but does not touch program
  // memory, so a pass that deletes, reorders, or hoists a call still must not
  // reorder it past another IO call.  IO is kept as a separate bit so that
  // memory optimisations (LICM load, DSE) can operate across IO calls while
  // the ADCE / dead-loop-elim passes still treat IO as live.
  bool reads_io{false};
  bool writes_io{false};

  // The callee may store a pointer argument into a location that outlives the
  // call, i.e. the pointer escapes.
  bool saves_pointer{false};

  // Per-pointer-argument effect.  Indexed by the call's operand position and
  // filled only for pointer-like arguments (Ptr to ...).  The values are the
  // same indices that the callee's formal parameter would have.
  enum class ArgEffect : uint8_t { kNone, kRead, kWrite, kReadWrite };
  std::vector<ArgEffect> arg_effects;

  // Does the callee read / write through any pointer argument?  These must be
  // folded into the coarse predicates below: a function whose only effect is a
  // store through a pointer parameter is NOT write-none, and treating it as
  // such lets ADCE delete the call outright.
  bool ReadsAnyArgument() const {
    return std::any_of(arg_effects.begin(), arg_effects.end(), [](ArgEffect e) {
      return e == ArgEffect::kRead || e == ArgEffect::kReadWrite;
    });
  }
  bool WritesAnyArgument() const {
    return std::any_of(arg_effects.begin(), arg_effects.end(), [](ArgEffect e) {
      return e == ArgEffect::kWrite || e == ArgEffect::kReadWrite;
    });
  }

  // IO-inclusive: used by ADCE / DeadLoopElim ("observable effect?").
  bool IsReadNone() const { return IsMemoryReadNone() && !reads_io; }
  bool IsWriteNone() const { return IsMemoryWriteNone() && !writes_io; }
  // Memory-only: used by LICM / DSE / MemorySSA / GVN ("does the call touch this pointer?").
  bool IsMemoryReadNone() const { return !reads_all_globals && read_globals.empty() && !ReadsAnyArgument(); }
  bool IsMemoryWriteNone() const { return !writes_all_globals && written_globals.empty() && !WritesAnyArgument(); }
};

class FunctionSideEffectPass : public AnalysisPass {
 public:
  FunctionSideEffectPass(context::CompilerContext *compiler_context, PassStage stage)
      : AnalysisPass(compiler_context, stage) {
    if (stage != PassStage::BASEIR) {
      throw std::invalid_argument("FunctionSideEffectPass only supports BASEIR stage");
    }
    AddDependency("CFGPass");
  }
  ~FunctionSideEffectPass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "FunctionSideEffectPass"; }

  // Public query interface.
  auto GetSummary(const std::string &func_name) const -> const SideEffectSummary &;
  bool HasSummary(const std::string &func_name) const;

  // True when the call has no observable effect (no IO, no memory writes).
  // Only meaningful when HasSummary(callee) is true.
  auto IsReadNoneAndIOClean(mlir::Operation *call) -> bool;

  // True for a function whose result depends only on its scalar arguments:
  // no memory access, no IO, and only calls to functions with the same property.
  // The greatest-fixed-point construction deliberately supports self and mutual
  // recursion, unlike the ordinary bottom-up effect summaries.
  auto IsReferentiallyTransparent(const std::string &func_name) const -> bool;

 private:
  auto ComputeOne(MLIRFunctionCFG *mlir_cfg) -> void;
  auto ComputeLibrary(const std::string &func_name) -> void;
  auto ComputeSCC(std::unordered_set<std::string> *scc_nodes, CFGPass *cfg_pass) -> void;
  auto ComputeReferentiallyTransparent(CFGPass *cfg_pass) -> void;

  std::unordered_map<std::string, SideEffectSummary> summaries_;
  std::unordered_set<std::string> referentially_transparent_;
};

}  // namespace middleend::pass::analysis

#endif  // MIDDLEEND_PASS_ANALYSIS_SIDEEFFECT_SIDEEFFECT_H
