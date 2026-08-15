#ifndef BACKEND_TARGETS_AARCH64_PASS_PEEPHOLE_H
#define BACKEND_TARGETS_AARCH64_PASS_PEEPHOLE_H

#include <string>
#include <unordered_map>
#include "backend/targets/aarch64/inst/aarch64inst.h"
#include "backend/targets/utils/pass/machinepass.h"

namespace backend::targetir {
class TargetContext;
class TargetBlock;
class TargetFunction;
class TargetOperand;
class TargetInst;
}  // namespace backend::targetir

namespace backend::targetir::aarch64 {

/**
 * @brief AArch64 machine peephole / instruction-combining pass (runs before
 * register allocation, on virtual registers). Mirrors RISCVPeephole:
 *   - immediate folding: `movz vC; add rd, rs, vC` -> `add rd, rs, #C`
 *     (and sub / cmp) when C fits the 12-bit arithmetic immediate;
 *   - strength reduction: multiply by a power of two -> `lsl`, by 1 -> `mov`,
 *     by 0 -> `movz #0`;
 *   - algebraic identities left behind (add/sub/lsl of 0 -> move).
 *
 * Logical immediates (and/orr/eor) are left in register form because AArch64's
 * bitmask-immediate encoding is not a simple range; folding them safely is a
 * later refinement. Dead `movz` constants are cleaned up by the DCE pass.
 */
class AArch64Peephole : public MachineFunctionPass {
 public:
  explicit AArch64Peephole(TargetContext *ctx, bool enable_tree_combine = true)
      : MachineFunctionPass(ctx), enable_tree_combine_(enable_tree_combine) {}

  auto Name() const -> std::string override { return "aarch64-peephole"; }
  auto RunOnFunction(TargetFunction *func) -> bool override;

 private:
  auto BuildAnalysis(TargetFunction *func) -> void;
  auto IsConst(TargetOperand *op, int *value) const -> bool;
  auto TransformInst(TargetInst *inst, TargetBlock *block) -> bool;
  auto TryFoldMultiplyAccumulate(TargetInst *inst, TargetBlock *block) -> bool;
  auto TryFoldShiftedArithmetic(TargetInst *inst, TargetBlock *block) -> bool;
  auto TryFoldExtendedArithmetic(TargetInst *inst, TargetBlock *block) -> bool;

  std::unordered_map<TargetOperand *, int> def_count_;
  std::unordered_map<TargetOperand *, int> use_count_;
  std::unordered_map<TargetOperand *, TargetInst *> defining_inst_;
  std::unordered_map<TargetInst *, TargetBlock *> containing_block_;
  std::unordered_map<TargetOperand *, int> const_value_;  // vreg -> value (single movz/movn def)
  bool enable_tree_combine_;
};

}  // namespace backend::targetir::aarch64

#endif
