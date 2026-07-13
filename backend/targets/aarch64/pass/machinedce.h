#ifndef BACKEND_TARGETS_AARCH64_PASS_MACHINEDCE_H
#define BACKEND_TARGETS_AARCH64_PASS_MACHINEDCE_H

#include <string>
#include "backend/targets/utils/pass/machinepass.h"

namespace backend::targetir {
class TargetContext;
class TargetFunction;
}  // namespace backend::targetir

namespace backend::targetir::aarch64 {

/**
 * @brief Machine dead-code elimination for AArch64: remove an instruction whose
 * result is an unused virtual register and which has no side effect (everything
 * except a call `bl`; stores/branches/cmp have no result). Iterated to a
 * fixpoint. Cleans up dead `movz` constants left by the peephole.
 */
class AArch64MachineDCE : public MachineFunctionPass {
 public:
  explicit AArch64MachineDCE(TargetContext *ctx) : MachineFunctionPass(ctx) {}

  auto Name() const -> std::string override { return "aarch64-machine-dce"; }
  auto RunOnFunction(TargetFunction *func) -> bool override;
};

}  // namespace backend::targetir::aarch64

#endif
