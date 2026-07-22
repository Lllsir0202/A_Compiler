#ifndef BACKEND_TARGETS_AARCH64_FRAME_AARCH64FRAME_H
#define BACKEND_TARGETS_AARCH64_FRAME_AARCH64FRAME_H

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>
#include "backend/targets/aarch64/inst/aarch64inst.h"
#include "backend/targets/utils/context/targetcontext.h"

namespace backend::targetir {
class TargetFunction;
class TargetInst;
}  // namespace backend::targetir

namespace backend::targetir::aarch64 {

/**
 * @brief AAPCS64 stack-frame layout + prologue/epilogue insertion. Runs after
 * register allocation. Mirrors RISCVFrame: outgoing-args + alloca at the bottom
 * (via sp), spill slots then callee-saved saves above, saved fp(x29)/lr(x30) at
 * the top, 16-byte aligned. fp is set to the incoming sp (sp + frame) so incoming
 * stack arguments are addressed as [fp, #k*8] independent of the frame size.
 */
class AArch64Frame {
 public:
  explicit AArch64Frame(TargetContext *ctx) : ctx_(ctx) {}

  auto Finalize(TargetFunction *func, size_t locals_bytes, size_t spill_slots,
                const std::vector<std::pair<TargetRegClass, uint8_t>> &callee_saved) -> void;

 private:
  auto MakeAddSp(int delta) -> std::vector<TargetInst *>;  // sp += delta (imm-legalized)
  auto MakeSetFp(int frame) -> std::vector<TargetInst *>;  // x29 = sp + frame
  auto EmitStackAccess(std::vector<TargetInst *> *out, A64Opcode op, TargetOperand *reg, int offset) -> void;
  auto MatOffset(std::vector<TargetInst *> *out, int offset) -> TargetOperand *;  // -> x16 = sp + offset

  TargetContext *ctx_;
};

}  // namespace backend::targetir::aarch64

#endif
