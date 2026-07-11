#ifndef BACKEND_TARGETS_RISCV_PASS_MACHINEDCE_H
#define BACKEND_TARGETS_RISCV_PASS_MACHINEDCE_H

#include <string>
#include "backend/targets/utils/pass/machinepass.h"

namespace backend::targetir {
class TargetContext;
class TargetFunction;
}  // namespace backend::targetir

namespace backend::targetir::riscv {

/**
 * @brief Machine dead-code elimination: remove an instruction whose result is a
 * virtual register that is never used, provided the instruction has no side
 * effect (everything except a call — stores, branches, jumps and returns have no
 * result and are therefore never candidates). Run to a fixpoint so a removal that
 * makes its operands' definitions dead is itself cleaned up.
 *
 * Runs before register allocation and mops up the dead `li` constants left behind
 * by the peephole pass, as well as any genuinely unused computation.
 */
class MachineDCE : public MachineFunctionPass {
 public:
  explicit MachineDCE(TargetContext *ctx) : MachineFunctionPass(ctx) {}

  auto Name() const -> std::string override { return "machine-dce"; }
  auto RunOnFunction(TargetFunction *func) -> bool override;
};

}  // namespace backend::targetir::riscv

#endif
