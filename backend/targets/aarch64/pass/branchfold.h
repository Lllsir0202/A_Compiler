#ifndef BACKEND_TARGETS_AARCH64_PASS_BRANCHFOLD_H
#define BACKEND_TARGETS_AARCH64_PASS_BRANCHFOLD_H

#include <string>
#include "backend/targets/utils/pass/machinepass.h"

namespace backend::targetir {
class TargetContext;
class TargetFunction;
}  // namespace backend::targetir

namespace backend::targetir::aarch64 {

/**
 * @brief Drop an unconditional `b` whose target is the block laid out immediately
 * after it (control falls through anyway). Blocks are emitted in function order.
 */
class AArch64BranchFold : public MachineFunctionPass {
 public:
  explicit AArch64BranchFold(TargetContext *ctx) : MachineFunctionPass(ctx) {}

  auto Name() const -> std::string override { return "aarch64-branch-fold"; }
  auto RunOnFunction(TargetFunction *func) -> bool override;
};

}  // namespace backend::targetir::aarch64

#endif
