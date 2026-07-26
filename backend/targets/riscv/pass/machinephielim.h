#ifndef BACKEND_TARGETS_RISCV_PASS_MACHINEPHIELIM_H
#define BACKEND_TARGETS_RISCV_PASS_MACHINEPHIELIM_H

#include <utility>
#include <vector>
#include "backend/targets/utils/pass/machinepass.h"

namespace backend::targetir {
class TargetBlock;
class TargetFunction;
class TargetInst;
class TargetOperand;
}  // namespace backend::targetir

namespace backend::targetir::riscv {

/**
 * Destroy machine SSA immediately before register allocation.
 *
 * Phi copies are grouped per CFG edge, critical edges are split, and each group
 * is sequentialized with a fresh virtual register when a parallel-copy cycle is
 * present. Keeping this pass late lets all pre-RA machine optimizations reason
 * about single-definition values.
 */
class MachinePhiElimination final : public MachineFunctionPass {
 public:
  explicit MachinePhiElimination(TargetContext *ctx) : MachineFunctionPass(ctx) {}

  auto Name() const -> std::string override { return "MachinePhiElimination"; }
  auto RunOnFunction(TargetFunction *func) -> bool override;

 private:
  using Copy = std::pair<TargetOperand *, TargetOperand *>;

  auto EmitParallelCopies(TargetBlock *block, std::vector<Copy> copies) -> void;
  auto InsertBeforeTerminators(TargetBlock *block, const std::vector<TargetInst *> &code) -> void;
};

}  // namespace backend::targetir::riscv

#endif
