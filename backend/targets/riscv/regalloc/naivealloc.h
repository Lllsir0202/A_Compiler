#ifndef BACKEND_TARGETS_RISCV_REGALLOC_NAIVEALLOC_H
#define BACKEND_TARGETS_RISCV_REGALLOC_NAIVEALLOC_H

#include <cstddef>
#include <unordered_map>
#include <vector>
#include "backend/targets/riscv/inst/riscvinst.h"
#include "backend/targets/utils/context/targetcontext.h"

namespace backend::targetir {
class TargetFunction;
class TargetInst;
class TargetVirtualReg;
}  // namespace backend::targetir

namespace backend::targetir::riscv {

/**
 * @brief Bring-up "spill-everything" register allocator: every virtual register
 * lives in its own stack slot, and each instruction reloads its operands into
 * scratch registers (t0-t2) and stores its result back. Correct for arbitrarily
 * many values with no interference analysis, but slow — it exists to make the
 * integer backend runnable end-to-end. The graph-coloring allocator
 * (backend/targets/utils/regalloc/graphcoloring.h) replaces it behind the same
 * pipeline slot without touching ISel or the frame layer.
 *
 * Reserves t0/t1/t2 as value scratch and t3 as address scratch (for spill
 * offsets outside the [-2048, 2047] immediate range), so those are never handed
 * out as value-carrying registers.
 */
class NaiveRegAlloc {
 public:
  explicit NaiveRegAlloc(TargetContext *ctx) : ctx_(ctx) {}

  /// Rewrite the function so no virtual registers remain. `alloca_base` is the
  /// byte offset where the spill area starts (above the alloca area). Returns the
  /// number of 8-byte spill slots used, which the frame layer turns into space.
  auto Run(TargetFunction *func, size_t alloca_base) -> size_t;

 private:
  auto SlotOf(TargetVirtualReg *vreg) -> size_t;
  // Append a load of `dst` <- slot, or a store of `src` -> slot, to `out`.
  auto EmitReload(std::vector<TargetInst *> *out, RVOpcode op, TargetOperand *dst, size_t slot) -> void;
  auto EmitSpill(std::vector<TargetInst *> *out, RVOpcode op, TargetOperand *src, size_t slot) -> void;

  TargetContext *ctx_;
  size_t alloca_base_ = 0;
  std::unordered_map<TargetVirtualReg *, size_t> slot_;
};

}  // namespace backend::targetir::riscv

#endif
