#ifndef BACKEND_TARGETS_AARCH64_PASS_SLPVECTORIZE_H
#define BACKEND_TARGETS_AARCH64_PASS_SLPVECTORIZE_H

#include <string>
#include "backend/targets/utils/pass/machinepass.h"

namespace backend::targetir {
class TargetContext;
class TargetFunction;
}  // namespace backend::targetir

namespace backend::targetir::aarch64 {

// Packs four adjacent i32 stores of one scalar value into an AdvSIMD duplicate
// plus one q store. This is the first deliberately small SLP seed: it operates
// on selected machine IR (not assembly text), requires exact base/offset
// equality, and leaves all non-proven groups untouched.
class AArch64SLPVectorize : public MachineFunctionPass {
 public:
  explicit AArch64SLPVectorize(TargetContext *ctx) : MachineFunctionPass(ctx) {}

  auto Name() const -> std::string override { return "aarch64-slp-vectorize"; }
  auto RunOnFunction(TargetFunction *func) -> bool override;
};

}  // namespace backend::targetir::aarch64

#endif
