#ifndef MIDDLEEND_PASS_TRANSFORM_MLIRLOOPUNSWITCH_MLIRLOOPUNSWITCH_H
#define MIDDLEEND_PASS_TRANSFORM_MLIRLOOPUNSWITCH_MLIRLOOPUNSWITCH_H

#include <cstddef>
#include <stdexcept>
#include <unordered_set>
#include <vector>
#include "middleend/mlir/utils/clone/operationcloner.h"
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
class IfOp;
class WhileOp;
}  // namespace middleend::mlir::scf

namespace middleend::pass::transform {

/**
 * @brief Hoist an invariant loop-body branch by cloning and specializing a
 * small structured loop under an outer `scf.if`.
 *
 * This is the non-trivial form of LLVM's SimpleLoopUnswitch. The condition
 * must be defined outside the loop, the loop must be safely cloneable, and a
 * cost cap prevents exponential code growth.
 */
class MLIRLoopUnswitchPass : public TransformPass {
 public:
  MLIRLoopUnswitchPass(context::CompilerContext *compiler_context, PassStage stage)
      : TransformPass(compiler_context, stage), cloner_(compiler_context) {
    if (stage != PassStage::MLIR) {
      throw std::invalid_argument("MLIRLoopUnswitchPass only supports MLIR stage");
    }
    AddDependency("MLIRMemoryEffectAnalysis");
  }
  ~MLIRLoopUnswitchPass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "MLIRLoopUnswitchPass"; }

 private:
  struct Candidate {
    size_t body_index{0};
    mlir::scf::IfOp *if_op{nullptr};
    mlir::Value *condition{nullptr};
    bool partial{false};
  };

  auto RunOnFunction(mlir::func::FuncDefOp *func_op) -> bool;
  auto UnswitchInBlock(mlir::func::FuncDefOp *func_op, mlir::Block *block) -> bool;
  auto FindCandidate(mlir::scf::WhileOp *loop, Candidate *candidate) -> bool;
  auto CanHoistInvariantCondition(mlir::scf::WhileOp *loop, mlir::Value *value,
                                  std::unordered_set<mlir::Value *> *visiting) -> bool;
  auto FindInvariantBoolean(mlir::scf::WhileOp *loop, mlir::Value *value, std::unordered_set<mlir::Value *> *visiting,
                            mlir::Value **candidate) -> bool;
  auto HoistInvariantCondition(mlir::scf::WhileOp *loop, mlir::Value *value,
                               std::vector<mlir::Operation *> *operations) -> mlir::Value *;
  auto Unswitch(mlir::func::FuncDefOp *func_op, mlir::Block *parent, size_t loop_index, mlir::scf::WhileOp *loop,
                const Candidate &candidate) -> void;
  auto Specialize(mlir::scf::WhileOp *loop, size_t body_index, bool take_then) -> void;

  mlir::utils::OperationCloner cloner_;
  analysis::MLIRMemoryEffectAnalysis *memory_effects_{nullptr};
  size_t candidates_{0};
  size_t unswitched_loops_{0};
  size_t loops_seen_{0};
  size_t body_ifs_seen_{0};
  size_t clone_or_cost_rejects_{0};
  size_t invariant_rejects_{0};
  static constexpr size_t kMaxLoopCost = 64;
};

}  // namespace middleend::pass::transform

#endif
