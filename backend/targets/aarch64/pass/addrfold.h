#ifndef BACKEND_TARGETS_AARCH64_PASS_ADDRFOLD_H
#define BACKEND_TARGETS_AARCH64_PASS_ADDRFOLD_H

#include <string>
#include <unordered_map>
#include "backend/targets/utils/pass/machinepass.h"

namespace backend::targetir {
class TargetContext;
class TargetFunction;
class TargetInst;
class TargetOperand;
}  // namespace backend::targetir

namespace backend::targetir::aarch64 {

/**
 * @brief Fold pointer arithmetic into AArch64 load/store addressing modes.
 *
 * Instruction selection always emits `ldr/str rt, [addr, #0]` with the effective
 * address fully materialized in `addr`. When `addr` is `base + #imm` or
 * `base + (index << s)` this pass rewrites the load/store to the immediate-offset
 * (`[base, #imm]`) or register-offset (`[base, index, lsl #s]`) form, dropping the
 * separate add/shift; MachineDCE then deletes the now-dead address arithmetic.
 * Relies on virtual registers being single-def (pre-regalloc SSA form).
 */
class AArch64AddrFold : public MachineFunctionPass {
 public:
  explicit AArch64AddrFold(TargetContext *ctx) : MachineFunctionPass(ctx) {}

  auto Name() const -> std::string override { return "aarch64-addr-fold"; }
  auto RunOnFunction(TargetFunction *func) -> bool override;

 private:
  auto BuildDefs(TargetFunction *func) -> void;
  auto SingleDef(TargetOperand *op) const -> TargetInst *;

  // vreg operand -> its unique defining instruction (only when defined exactly once).
  std::unordered_map<TargetOperand *, TargetInst *> def_;
  std::unordered_map<TargetOperand *, int> def_count_;
};

}  // namespace backend::targetir::aarch64

#endif
