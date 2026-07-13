#ifndef BACKEND_TARGETS_AARCH64_PASS_LOCALCSE_H
#define BACKEND_TARGETS_AARCH64_PASS_LOCALCSE_H

#include <string>
#include "backend/targets/utils/pass/machinepass.h"

namespace backend::targetir {
class TargetContext;
class TargetFunction;
class TargetInst;
}  // namespace backend::targetir

namespace backend::targetir::aarch64 {

/**
 * @brief Local (per-basic-block) common-subexpression elimination over pure
 * register-to-register instructions. Instruction selection re-emits the same
 * address arithmetic for every access -- notably an `sxtw` of the same index in
 * each unrolled loop body -- which the middle-end GVN never sees because it is
 * created here. Within a block the first result of a given (opcode, operands)
 * becomes canonical and later identical computations are folded onto it (then
 * removed), which is always safe: virtual registers are single-def, so operands
 * keep their value, and the canonical dominates the duplicates it replaces.
 *
 * Only pure ops with all-virtual/immediate/symbol operands are considered:
 * loads/stores (memory state), flag setters (cmp/cset), and anything reading a
 * physical register (args, sp, return values that may be reassigned) are skipped.
 */
class AArch64LocalCSE : public MachineFunctionPass {
 public:
  explicit AArch64LocalCSE(TargetContext *ctx) : MachineFunctionPass(ctx) {}

  auto Name() const -> std::string override { return "aarch64-local-cse"; }
  auto RunOnFunction(TargetFunction *func) -> bool override;

 private:
  // A canonical key for a CSE-able instruction, or empty if it must not be CSE'd.
  static auto KeyOf(TargetInst *inst) -> std::string;
};

}  // namespace backend::targetir::aarch64

#endif
