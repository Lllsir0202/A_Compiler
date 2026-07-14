#ifndef BACKEND_TARGETS_RISCV_PASS_BLOCKLAYOUT_H
#define BACKEND_TARGETS_RISCV_PASS_BLOCKLAYOUT_H

#include <string>
#include <unordered_set>
#include "backend/targets/utils/pass/machinepass.h"

namespace backend::targetir {
class TargetFunction;
}

namespace backend::targetir::riscv {

/** Greedy CFG trace placement that favors true/body fallthrough edges. */
class RISCVBlockLayout : public MachineFunctionPass {
 public:
  explicit RISCVBlockLayout(TargetContext *ctx) : MachineFunctionPass(ctx) {}

  auto Name() const -> std::string override { return "riscv-block-layout"; }
  auto RunOnFunction(TargetFunction *func) -> bool override;

 private:
  std::unordered_set<TargetFunction *> processed_;
};

}  // namespace backend::targetir::riscv

#endif
