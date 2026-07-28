#ifndef BACKEND_TARGETS_RISCV_PASS_TWOADDRESS_H
#define BACKEND_TARGETS_RISCV_PASS_TWOADDRESS_H

#include <string>
#include "backend/targets/utils/pass/machinepass.h"

namespace backend::targetir {
class TargetContext;
class TargetFunction;
}  // namespace backend::targetir

namespace backend::targetir::riscv {

/** Fold `tmp = op ...; dst = mv tmp` into `dst = op ...` before allocation. */
class RISCVTwoAddress : public MachineFunctionPass {
 public:
  explicit RISCVTwoAddress(TargetContext *ctx) : MachineFunctionPass(ctx) {}

  auto Name() const -> std::string override { return "riscv-two-address"; }
  auto RunOnFunction(TargetFunction *func) -> bool override;
};

}  // namespace backend::targetir::riscv

#endif
