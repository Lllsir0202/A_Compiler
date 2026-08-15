#ifndef MIDDLEEND_PASS_ANALYSIS_LOOPACCESS_LOOPACCESS_H
#define MIDDLEEND_PASS_ANALYSIS_LOOPACCESS_LOOPACCESS_H

#include <cstddef>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include "middleend/pass/analysis/analysis.h"

namespace middleend::mlir {
class Block;
class Value;
}  // namespace middleend::mlir
namespace middleend::pass::analysis {
class AliasAnalysisPass;
class Loop;
class LoopInfoPass;
class SCEV;
class SCEVPass;

// Analysis-only foundation for loop vectorization.  It records the address
// recurrence shape of each load/store without changing IR; later vector and
// dependence transforms share this conservative description.
class LoopAccessAnalysisPass : public AnalysisPass {
 public:
  enum class AccessKind : unsigned char { Load, Store };
  struct Access {
    AccessKind kind;
    mlir::Value *pointer;
    mlir::Value *root;
    bool affine;
    bool has_loop_variant_index;
  };
  enum class DependenceKind : unsigned char { NoDependence, IntraIteration, LoopCarried, Unknown };
  struct Dependence {
    size_t first;
    size_t second;
    DependenceKind kind;
    int distance;  // valid only for a proven constant loop-carried distance
  };

  LoopAccessAnalysisPass(context::CompilerContext *compiler_context, PassStage stage)
      : AnalysisPass(compiler_context, stage) {
    if (stage != PassStage::BASEIR) throw std::invalid_argument("LoopAccessAnalysisPass only supports BASEIR stage");
    AddDependency("CFGPass");
    AddDependency("LoopInfoPass");
    AddDependency("SCEVPass");
    AddDependency("AliasAnalysisPass");
  }
  ~LoopAccessAnalysisPass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "LoopAccessAnalysisPass"; }
  auto GetAccesses(Loop *loop) const -> const std::vector<Access> &;
  auto GetDependences(Loop *loop) const -> const std::vector<Dependence> &;

 private:
  auto RecordAccess(mlir::Value *pointer, AccessKind kind, Loop *loop) -> void;
  auto ClassifyDependence(const Access &first, const Access &second, Loop *loop) -> Dependence;
  auto ConstantDistance(mlir::Value *first, mlir::Value *second, Loop *loop, int *distance) const -> bool;
  auto ConstantDifference(const SCEV *first, const SCEV *second, int *difference) const -> bool;

  LoopInfoPass *loop_info_{nullptr};
  SCEVPass *scev_{nullptr};
  AliasAnalysisPass *alias_{nullptr};
  std::unordered_map<Loop *, std::vector<Access>> accesses_;
  std::unordered_map<Loop *, std::vector<Dependence>> dependences_;
  size_t loops_analyzed_{0};
  size_t affine_accesses_{0};
  size_t rejected_non_gep_{0};
  size_t rejected_non_affine_{0};
  size_t no_dependence_pairs_{0};
  size_t intra_iteration_pairs_{0};
  size_t loop_carried_pairs_{0};
  size_t unknown_pairs_{0};
};
}  // namespace middleend::pass::analysis

#endif
