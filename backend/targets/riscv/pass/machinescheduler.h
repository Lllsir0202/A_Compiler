#ifndef BACKEND_TARGETS_RISCV_PASS_MACHINESCHEDULER_H
#define BACKEND_TARGETS_RISCV_PASS_MACHINESCHEDULER_H

#include <string>
#include "backend/targets/utils/pass/machinepass.h"

namespace backend::targetir {
class TargetContext;
class TargetFunction;
}  // namespace backend::targetir

namespace backend::targetir::riscv {

/**
 * @brief Deterministic local list scheduler for machine-SSA code.
 *
 * Builds a dependence DAG for each straight-line region of a basic block.
 * Register RAW/WAR/WAW dependencies and a conservative memory chain preserve
 * semantics; calls, terminators and other pinned pseudos delimit regions. The
 * ready-list priority exposes load and arithmetic latency while preferring
 * choices that release operands, limiting unnecessary live-range growth.
 */
class MachineScheduler final : public MachineFunctionPass {
 public:
  explicit MachineScheduler(TargetContext *ctx) : MachineFunctionPass(ctx) {}

  auto Name() const -> std::string override { return "machine-scheduler"; }
  auto RunOnFunction(TargetFunction *function) -> bool override;
};

}  // namespace backend::targetir::riscv

#endif
