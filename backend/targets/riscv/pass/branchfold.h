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
 * @brief Branch folding / block layout cleanup: drop an unconditional `j` whose
 * target is the block laid out immediately after it, since control falls through
 * to it anyway. Blocks are emitted in function order, so this is always safe.
 */
class BranchFold : public MachineFunctionPass {
 public:
  explicit BranchFold(TargetContext *ctx) : MachineFunctionPass(ctx) {}

  auto Name() const -> std::string override { return "riscv-branch-fold"; }
  auto RunOnFunction(TargetFunction *func) -> bool override;
};

}  // namespace backend::targetir::riscv

#endif
