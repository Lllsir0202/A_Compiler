#ifndef BACKEND_TARGETS_RISCV_PASS_BRANCHFOLD_H
#define BACKEND_TARGETS_RISCV_PASS_BRANCHFOLD_H

#include <string>
#include "backend/targets/utils/pass/machinepass.h"

namespace backend::targetir {
class TargetContext;
class TargetFunction;
}  // namespace backend::targetir

namespace backend::targetir::riscv {

/**
 * @brief Branch folding / block layout cleanup.
 *
 * Drop an unconditional `j` to the next block. Also invert `bcond true; j false`
 * when `true` is the next block, turning it into `b!cond false` plus fallthrough.
 * Blocks are emitted in function order, so both rewrites preserve the CFG.
 */
class BranchFold : public MachineFunctionPass {
 public:
  explicit BranchFold(TargetContext *ctx) : MachineFunctionPass(ctx) {}

  auto Name() const -> std::string override { return "riscv-branch-fold"; }
  auto RunOnFunction(TargetFunction *func) -> bool override;
};

}  // namespace backend::targetir::riscv

#endif
