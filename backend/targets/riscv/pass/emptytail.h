#ifndef BACKEND_TARGETS_RISCV_PASS_EMPTYTAIL_H
#define BACKEND_TARGETS_RISCV_PASS_EMPTYTAIL_H

namespace backend::targetir {
class TargetFunction;
}

namespace backend::targetir::riscv {

/**
 * Bypass a monotone counted-loop tail which only advances a dead induction
 * variable until the loop bound.  This runs after register allocation so the
 * exact control-flow/copy shape produced by phi destruction is visible.
 */
class RISCVEmptyTail {
 public:
  auto RunOnFunction(TargetFunction *func) -> bool;
};

}  // namespace backend::targetir::riscv

#endif
