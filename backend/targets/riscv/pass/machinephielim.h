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
 * Edge preparation and SSA destruction are separate operations. PrepareEdges
 * materializes edge-local code and splits critical edges while retaining Phi
 * nodes; RunOnFunction then groups Phi copies per prepared edge and
 * sequentializes each parallel-copy set.
 */
class MachinePhiElimination final : public MachineFunctionPass {
 public:
  explicit MachinePhiElimination(TargetContext *ctx) : MachineFunctionPass(ctx) {}

  auto Name() const -> std::string override { return "MachinePhiElimination"; }
  auto PrepareEdges(TargetFunction *func) -> bool;
  auto RunOnFunction(TargetFunction *func) -> bool override;

 private:
  using Copy = std::pair<TargetOperand *, TargetOperand *>;

  auto EmitParallelCopies(TargetBlock *block, std::vector<Copy> copies) -> void;
  auto InsertBeforeTerminators(TargetBlock *block, const std::vector<TargetInst *> &code) -> void;
};

}  // namespace backend::targetir::riscv

#endif
