#ifndef BACKEND_TARGETS_RISCV_PASS_MACHINELICM_H
#define BACKEND_TARGETS_RISCV_PASS_MACHINELICM_H

#include <string>
#include "backend/targets/utils/pass/machinepass.h"

namespace backend::targetir {
class TargetContext;
class TargetFunction;
}  // namespace backend::targetir

namespace backend::targetir::riscv {

/**
 * @brief Machine-level loop-invariant code motion for pure register-to-register
 * instructions.
 *
 * Instruction selection materializes address arithmetic (`slli` of an index,
 * `add` of a base) at every array access. When such a chain only reads values
 * that do not change inside a loop -- e.g. `&A[i][j]` where `i` and the base are
 * fixed across the inner `k` loop -- the middle-end LICM cannot help: it never
 * saw these operations, which are created here after lowering. This pass hoists
 * that invariant arithmetic into the loop preheader so it runs once per loop
 * entry instead of once per iteration.
 *
 * Runs before register allocation, where results are single-assignment virtual
 * registers. Correctness is kept conservative and rigorous:
 *   - Loops are natural loops of a back-edge (header dominates latch), with the
 *     body computed by backward reachability, so no reliance on block layout.
 *   - Only pure ops are moved (the same set as local CSE, minus division, which
 *     is expensive to speculate); loads/stores keep their memory ordering and
 *     the aliasing load in a matrix kernel is deliberately left in place.
 *   - An instruction is invariant when none of its operands are defined inside
 *     the loop (or are themselves hoisted), and its result has a single
 *     definition, so removing it from the body cannot drop a needed value.
 *   - Code is hoisted only when the header has exactly one predecessor outside
 *     the loop -- a dedicated preheader that dominates the header -- and is
 *     inserted before that block's terminator.
 *   - Hoisting is register-pressure aware. Multi-input arithmetic can let its
 *     now loop-dead source operands be freed, so it is hoisted freely. A
 *     trivially rematerializable `li`/`la` frees nothing and adds one value live
 *     across the whole loop, so it is hoisted only while the loop's estimated
 *     peak pressure leaves headroom below the register file. This avoids trading
 *     a one-instruction recompute for a hot-loop spill.
 *
 * The pass manager re-runs the machine pipeline to a fixpoint, so an invariant
 * chain hoisted out of an inner loop is hoisted again out of the enclosing loop
 * on a later iteration without any explicit nesting handling here.
 */
class MachineLICM : public MachineFunctionPass {
 public:
  explicit MachineLICM(TargetContext *ctx) : MachineFunctionPass(ctx) {}

  auto Name() const -> std::string override { return "riscv-machine-licm"; }
  auto RunOnFunction(TargetFunction *func) -> bool override;
};

}  // namespace backend::targetir::riscv

#endif
