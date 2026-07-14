#ifndef BACKEND_TARGETS_AARCH64_PASS_IFCONVERT_H
#define BACKEND_TARGETS_AARCH64_PASS_IFCONVERT_H

#include <string>
#include "backend/targets/utils/pass/machinepass.h"

namespace backend::targetir {
class TargetContext;
class TargetFunction;
}  // namespace backend::targetir

namespace backend::targetir::aarch64 {

/**
 * @brief If-conversion: turn a small control-flow diamond that merely selects
 * between two computed values into branchless `csel` instructions. Removing the
 * conditional branch avoids a mispredict on the out-of-order judging cores (a
 * data-dependent branch such as `if (x < 0) x = -x;` is inherently unpredictable).
 *
 * Runs first in the machine pipeline (before branch folding), on the canonical
 * shape instruction selection emits for an `if`/`else` that yields a value:
 *
 *   H:  ... ; cmp a, b ; b.<cc> T ; b F            (F/T are H's only successors)
 *   T:  <speculatable pure ops> ; mov Vk, sTk ... ; b M
 *   F:  <speculatable pure ops> ; mov Vk, sFk ... ; b M
 *   M:  ... uses Vk ...
 *
 * where T and F each have H as their sole predecessor, contain no memory / call /
 * flag-setting op, and merge the same phi values {Vk} (each defined by a register
 * move -- the parallel copy phi-elimination emits). The pure ops of both arms are
 * hoisted into H (safe: side-effect-free and non-faulting) and each Vk becomes
 * `csel Vk, sTk, sFk, cc`. Anything not matching this shape exactly is left alone.
 */
class AArch64IfConvert : public MachineFunctionPass {
 public:
  explicit AArch64IfConvert(TargetContext *ctx) : MachineFunctionPass(ctx) {}

  auto Name() const -> std::string override { return "aarch64-if-convert"; }
  auto RunOnFunction(TargetFunction *func) -> bool override;
};

}  // namespace backend::targetir::aarch64

#endif
