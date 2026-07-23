#ifndef MIDDLEEND_PASS_TRANSFORM_MLIRLOOPFUSION_MLIRLOOPFUSION_H
#define MIDDLEEND_PASS_TRANSFORM_MLIRLOOPFUSION_MLIRLOOPFUSION_H

#include <cstddef>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "middleend/pass/transform/transform.h"

namespace middleend::pass::analysis {
class MLIRMemoryEffectAnalysis;
}

namespace middleend::mlir {
class Block;
class Operation;
class Value;
}  // namespace middleend::mlir

namespace middleend::mlir::func {
class FuncDefOp;
}

namespace middleend::mlir::scf {
class WhileOp;
}

namespace middleend::pass::transform {
namespace analysis = middleend::pass::analysis;

/**
 * @brief Fuse adjacent structured counted loops with the same iteration domain.
 *
 * The legality model follows LLVM LoopFuse: equal trip counts, adjacency, no
 * scalar dependence through the first loop's final values, and no backward
 * loop-carried memory dependence. The structured representation lets the pass
 * combine loop-carried values directly, before CFG lowering introduces phis.
 */
class MLIRLoopFusionPass : public TransformPass {
 public:
  MLIRLoopFusionPass(context::CompilerContext *compiler_context, PassStage stage)
      : TransformPass(compiler_context, stage) {
    if (stage != PassStage::MLIR) {
      throw std::invalid_argument("MLIRLoopFusionPass only supports MLIR stage");
    }
    AddDependency("MLIRMemoryEffectAnalysis");
  }
  ~MLIRLoopFusionPass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "MLIRLoopFusionPass"; }

 private:
  struct MemoryAccess {
    mlir::Value *pointer{nullptr};
    bool writes{false};
  };

  auto RunOnFunction(mlir::func::FuncDefOp *func_op) -> bool;
  auto CollectInterproceduralNoAliasFacts() -> void;
  auto AreProvenNoAliasParameters(mlir::Value *lhs, mlir::Value *rhs) const -> bool;
  auto FuseInBlock(mlir::Block *block) -> bool;
  auto CanFuse(mlir::scf::WhileOp *first, mlir::scf::WhileOp *second) -> bool;
  auto DependencesAllowFusion(mlir::scf::WhileOp *first, mlir::scf::WhileOp *second,
                              const std::unordered_map<mlir::Value *, mlir::Value *> &argument_map,
                              bool aligned_induction_values) -> bool;
  auto CollectMemoryAccesses(mlir::Operation *operation, std::vector<MemoryAccess> *accesses) -> bool;
  auto Fuse(mlir::Block *block, size_t first_index, mlir::scf::WhileOp *first, mlir::scf::WhileOp *second) -> void;

  size_t fused_loops_{0};
  size_t candidates_{0};
  size_t domain_rejects_{0};
  size_t shape_rejects_{0};
  size_t scalar_rejects_{0};
  size_t memory_rejects_{0};
  std::unordered_map<mlir::Value *, std::unordered_set<mlir::Value *>> noalias_parameters_;
  analysis::MLIRMemoryEffectAnalysis *memory_effects_{nullptr};
};

}  // namespace middleend::pass::transform

#endif
