#ifndef BACKEND_TARGETS_AARCH64_PASS_MACHINELICM_H
#define BACKEND_TARGETS_AARCH64_PASS_MACHINELICM_H

#include <string>
#include "backend/targets/utils/pass/machinepass.h"

namespace backend::targetir {
class TargetContext;
class TargetFunction;
}  // namespace backend::targetir

namespace backend::targetir::aarch64 {

/**
 * Hoist pure loop-invariant arithmetic introduced by AArch64 instruction
 * selection.  In particular, this exposes row bases and constant-stride address
 * chains which the middle-end LICM cannot see.
 */
class AArch64MachineLICM : public MachineFunctionPass {
 public:
  explicit AArch64MachineLICM(TargetContext *ctx) : MachineFunctionPass(ctx) {}

  auto Name() const -> std::string override { return "aarch64-machine-licm"; }
  auto RunOnFunction(TargetFunction *func) -> bool override;
};

}  // namespace backend::targetir::aarch64

#endif
