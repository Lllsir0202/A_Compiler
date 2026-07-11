#ifndef BACKEND_TARGETS_UTILS_PASS_MACHINEPASS_H
#define BACKEND_TARGETS_UTILS_PASS_MACHINEPASS_H

#include <string>

namespace backend::targetir {
class TargetContext;
class TargetFunction;

/**
 * @brief Base class for a machine-level (post instruction-selection) optimization
 * pass, analogous to the middle-end `middleend::pass::Pass`. A pass transforms a
 * single TargetFunction and reports whether it changed anything, which lets the
 * pass manager iterate a peephole pipeline to a fixpoint.
 *
 * Passes are target-independent in interface; concrete passes (RISC-V peephole,
 * dead-code elimination, branch folding, ...) live under the target directory and
 * may inspect target-specific opcodes. They are assembled and toggled by the
 * backend driver based on the optimization level, exactly like the middle-end
 * pass pipeline in main.cpp.
 */
class MachineFunctionPass {
 public:
  virtual ~MachineFunctionPass() = default;

  /// Human-readable pass name (for debugging / pass listing).
  virtual auto Name() const -> std::string = 0;

  /// Run the pass on one function. Returns true if the function was modified.
  virtual auto RunOnFunction(TargetFunction *func) -> bool = 0;

 protected:
  explicit MachineFunctionPass(TargetContext *ctx) : ctx_(ctx) {}
  TargetContext *ctx_;
};

}  // namespace backend::targetir

#endif
