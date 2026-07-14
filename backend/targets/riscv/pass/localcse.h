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
 * @brief Local (per-basic-block) common-subexpression elimination over pure
 * register-to-register instructions. Instruction selection re-emits the same
 * address arithmetic for every access -- notably a `sext.w` / `slli` of the same
 * index in each unrolled loop body -- which the middle-end GVN never sees because
 * it is created here. Within a block the first result of a given (opcode,
 * operands) becomes canonical and later identical computations are folded onto it
 * (then removed).
 *
 * Only pure ops with all-virtual/immediate/symbol operands are considered:
 * loads/stores (memory state), branches, and anything reading a physical register
 * (args, sp, return values that may be reassigned) are skipped. Backend virtual
 * registers are NOT single-assignment (loop phis / accumulators are multiply
 * defined), so only single-def results are folded and cached expressions are
 * invalidated when an operand register is redefined mid-block.
 */
class RISCVLocalCSE : public MachineFunctionPass {
 public:
  explicit RISCVLocalCSE(TargetContext *ctx) : MachineFunctionPass(ctx) {}

  auto Name() const -> std::string override { return "riscv-local-cse"; }
  auto RunOnFunction(TargetFunction *func) -> bool override;

 private:
  // A canonical key for a CSE-able instruction, or empty if it must not be CSE'd.
  static auto KeyOf(TargetInst *inst) -> std::string;
};

}  // namespace backend::targetir::riscv

#endif
