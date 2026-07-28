#ifndef BACKEND_TARGETS_RISCV_PASS_POSTRA_H
#define BACKEND_TARGETS_RISCV_PASS_POSTRA_H

#include <cstddef>

namespace backend::targetir {
class TargetContext;
class TargetFunction;
}  // namespace backend::targetir

namespace backend::targetir::riscv {

/**
 * Clean up artifacts left by physical-register assignment.
 *
 * The spill optimization is deliberately restricted to the private frame
 * slots allocated by GraphColorRegAlloc.  Local arrays, outgoing arguments,
 * callee-save slots, and arbitrary SP-relative user memory are never touched.
 */
class RISCVPostRA {
 public:
  explicit RISCVPostRA(TargetContext *ctx) : ctx_(ctx) {}

  auto RunOnFunction(TargetFunction *func, size_t spill_base, size_t spill_slots) -> bool;

 private:
  TargetContext *ctx_;
};

}  // namespace backend::targetir::riscv

#endif
