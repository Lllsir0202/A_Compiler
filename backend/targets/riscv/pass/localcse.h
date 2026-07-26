#ifndef BACKEND_TARGETS_RISCV_PASS_LOCALCSE_H
#define BACKEND_TARGETS_RISCV_PASS_LOCALCSE_H

#include <string>
#include "backend/targets/utils/pass/machinepass.h"

namespace backend::targetir {
class TargetContext;
class TargetFunction;
class TargetInst;
}  // namespace backend::targetir

namespace backend::targetir::riscv {

/**
 * @brief Dominator-scoped common-subexpression elimination over pure
 * register-to-register instructions. Instruction selection re-emits the same
 * address arithmetic for every access -- notably a `sext.w` / `slli` of the same
 * index in each unrolled loop body -- which the middle-end GVN never sees because
 * it is created here. Within a block the first result of a given (opcode,
 * operands) becomes canonical and later identical computations are folded onto it
 * (then removed). Machine SSA makes virtual-register operands stable, so an
 * expression remains available in dominated blocks without pass-local
 * invalidation. Commutative operands are canonicalized. A cross-block
 * replacement is accepted only when the canonical value is already live-in;
 * local replacements may not extend a non-rematerializable live range.
 *
 * Only pure ops with all-virtual/immediate/symbol operands are considered:
 * loads/stores (memory state), branches, and anything reading a physical register
 * (args, sp, return values that may be reassigned) are skipped. Backend virtual
 * registers are required to be single-assignment by the machine verifier.
 */
class RISCVLocalCSE : public MachineFunctionPass {
 public:
  explicit RISCVLocalCSE(TargetContext *ctx) : MachineFunctionPass(ctx) {}

  auto Name() const -> std::string override { return "riscv-local-cse"; }
  auto RunOnFunction(TargetFunction *func) -> bool override;

  // A canonical key for a CSE-able instruction, or empty if it must not be CSE'd.
  static auto KeyOf(TargetInst *inst) -> std::string;
};

/**
 * Compatibility cleanup after Phi elimination. Until register allocation
 * coalescing consumes SSA edge copies directly, this pass retains the proven
 * multiply-defined-register invalidation rules used by the old pipeline.
 */
class RISCVPostPhiLocalCSE : public MachineFunctionPass {
 public:
  explicit RISCVPostPhiLocalCSE(TargetContext *ctx) : MachineFunctionPass(ctx) {}

  auto Name() const -> std::string override { return "riscv-post-phi-local-cse"; }
  auto RunOnFunction(TargetFunction *func) -> bool override;
};

}  // namespace backend::targetir::riscv

#endif
