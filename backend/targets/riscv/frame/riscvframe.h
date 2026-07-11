#ifndef BACKEND_TARGETS_RISCV_FRAME_RISCVFRAME_H
#define BACKEND_TARGETS_RISCV_FRAME_RISCVFRAME_H

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>
#include "backend/targets/riscv/inst/riscvinst.h"
#include "backend/targets/utils/context/targetcontext.h"

namespace backend::targetir {
class TargetFunction;
class TargetInst;
}  // namespace backend::targetir

namespace backend::targetir::riscv {

/**
 * @brief Stack-frame layout + prologue/epilogue insertion for a RISC-V function
 * (lp64d). Runs after register allocation, once the spill-slot count is known.
 *
 * Frame (from sp after the prologue, low -> high address):
 *   [0, spill_bytes)        spill slots (assigned by the allocator)
 *   [frame - 8, frame)      saved return address (ra)
 * 16-byte aligned. `ret` instructions are expanded to restore ra, release the
 * frame, and return. Callee-saved s-registers need no saving yet because the
 * bring-up allocator only uses caller-saved scratch (t0-t3) and ABI argument
 * registers; the graph-coloring allocator will report its used callee-saved set
 * for this layer to preserve.
 */
class RISCVFrame {
 public:
  explicit RISCVFrame(TargetContext *ctx) : ctx_(ctx) {}

  /// Finalize the frame. `locals_bytes` covers the outgoing-arg + alloca areas
  /// at the bottom of the frame; `spill_slots` 8-byte slots sit above them, and
  /// the `callee_saved` (class,id) registers the allocator handed out are saved
  /// above the spill area and restored in the epilogue.
  auto Finalize(TargetFunction *func, size_t locals_bytes, size_t spill_slots,
                const std::vector<std::pair<TargetRegClass, uint8_t>> &callee_saved) -> void;

 private:
  auto MakeAddSp(int delta) -> std::vector<TargetInst *>;  // sp += delta (imm-legalized)
  auto MakeSetFp(int frame) -> std::vector<TargetInst *>;  // fp = sp + frame (imm-legalized)
  // Append a legalized `op reg, offset(sp)` (SD store or LD load) to `out`.
  auto EmitStackAccess(std::vector<TargetInst *> *out, RVOpcode op, TargetOperand *reg, int offset) -> void;

  TargetContext *ctx_;
};

}  // namespace backend::targetir::riscv

#endif
