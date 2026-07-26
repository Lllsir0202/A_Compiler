#ifndef BACKEND_TARGETS_RISCV_PASS_PEEPHOLE_H
#define BACKEND_TARGETS_RISCV_PASS_PEEPHOLE_H

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>
#include "backend/targets/riscv/inst/riscvinst.h"
#include "backend/targets/utils/pass/machinepass.h"

namespace backend::targetir {
class TargetContext;
class TargetFunction;
class TargetOperand;
class TargetInst;
class TargetBlock;
}  // namespace backend::targetir

namespace backend::targetir::riscv {

/**
 * @brief RISC-V machine peephole / instruction-combining pass (runs before
 * register allocation, on virtual registers).
 *
 * Rewrites individual instructions using constants materialized by `li`:
 *   - immediate folding: `li vC; addw rd, rs, vC`  ->  `addiw rd, rs, C`
 *     (and the sub / and / or / xor / shift / slt analogues), whenever C fits the
 *     12-bit signed immediate (or 0..31 for a shift amount);
 *   - strength reduction: multiply by a power of two -> shift, by 1 -> move,
 *     by 0 -> load-zero;
 *   - algebraic identities left behind by the above (add/or/xor/shift of 0 -> move,
 *     and of -1 -> move).
 *   - redundant RV64 word sign extension -> coalescible move, when the source
 *     has one definition and that instruction already sign-extends to XLEN.
 *   - zero-valued equality-branch operands -> the architectural zero register,
 *     exposing their now-dead `li 0` definitions to machine DCE.
 *   - materialized integer comparisons consumed by equality branches -> direct
 *     RISC-V comparison branches, including an intervening boolean inversion.
 *
 * The now-dead `li` instructions are cleaned up by the dead-code pass. Nothing
 * here special-cases a test shape; every rewrite is a general algebraic identity
 * of the RISC-V instruction it matches.
 */
class RISCVPeephole : public MachineFunctionPass {
 public:
  explicit RISCVPeephole(TargetContext *ctx, bool ssa_safe_mode = false)
      : MachineFunctionPass(ctx), ssa_safe_mode_(ssa_safe_mode) {}

  auto Name() const -> std::string override { return "riscv-peephole"; }
  auto RunOnFunction(TargetFunction *func) -> bool override;

 private:
  // Rebuild single-definition and constant facts for virtual registers.
  auto BuildFacts(TargetFunction *func) -> void;
  // If `op` is a register holding a known constant, store it in `*value`.
  auto IsConst(TargetOperand *op, int *value) const -> bool;
  // Whether the single instruction defining `op` already sign-extends its
  // 32-bit result to XLEN, making a following `sext.w` redundant on RV64.
  auto HasSignExtendedWordResult(TargetOperand *op) const -> bool;
  // Replace known-zero operands of an equality branch with x0.
  auto FoldBranchZero(TargetInst *inst) -> bool;
  // Fold `slt[/u]`/`seqz`/`snez` boolean values into their consuming branch.
  auto FoldCompareBranch(TargetInst *inst) -> bool;
  // Whether every register operand in `ops` holds the same value at `use` as at
  // `def` (safe to relocate the operands of `def` down to `use`).
  auto OperandsStableBetween(TargetInst *def, TargetInst *use, const std::vector<TargetOperand *> &ops) const -> bool;
  // Try to rewrite one instruction; returns true if it changed.
  auto TransformInst(TargetInst *inst) -> bool;

  std::unordered_map<TargetOperand *, int> def_count_;
  std::unordered_map<TargetOperand *, int> use_count_;
  std::unordered_map<TargetOperand *, int> li_value_;
  std::unordered_map<TargetOperand *, TargetInst *> defining_inst_;
  std::unordered_map<TargetInst *, TargetBlock *> inst_block_;
  std::unordered_map<TargetInst *, size_t> inst_index_;
  bool ssa_safe_mode_ = false;
};

}  // namespace backend::targetir::riscv

#endif
