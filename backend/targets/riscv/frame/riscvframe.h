#ifndef BACKEND_TARGETS_RISCV_FRAME_RISCVFRAME_H
#define BACKEND_TARGETS_RISCV_FRAME_RISCVFRAME_H

namespace backend::targetir {
class TargetFunction;
}

namespace backend::targetir::riscv {

/**
 * @brief Stack-frame layout, prologue/epilogue insertion (PEI) and the lp64d
 * calling convention for a RISC-V function. Runs after register allocation, when
 * the set of used callee-saved registers and the spill-slot count are known.
 *
 * Frame layout (high -> low address), see docs/backend_codegen_plan.md §4:
 *   incoming stack args | ra | saved fp | callee-saved s/fs regs | spill slots
 *   plus locals/allocas | outgoing stack args
 * 16-byte aligned. The prologue `addi sp,sp,-frame` uses a legalized immediate
 * (li + sub when the frame exceeds the addi range).
 *
 * Skeleton: declares the contract; bodies land in milestone M2.
 */
class RISCVFrame {
 public:
  /// Finalize the frame: assign FrameIndex slots to sp offsets, emit prologue /
  /// epilogue, save/restore ra + used callee-saved registers.
  auto Finalize(TargetFunction *func) -> void;
};

}  // namespace backend::targetir::riscv

#endif
