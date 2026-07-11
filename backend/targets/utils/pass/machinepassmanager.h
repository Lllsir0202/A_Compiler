#ifndef BACKEND_TARGETS_UTILS_PASS_MACHINEPASSMANAGER_H
#define BACKEND_TARGETS_UTILS_PASS_MACHINEPASSMANAGER_H

#include <vector>
#include "backend/targets/utils/pass/machinepass.h"

namespace backend::targetir {
class TargetContext;
class TargetFunction;

/**
 * @brief Owns and runs an ordered list of machine-level passes over a function,
 * mirroring the middle-end `PassManager`. Because peephole passes can expose new
 * opportunities for one another (fold -> dead code -> fold), the manager re-runs
 * the pipeline until it reaches a fixpoint (no pass reports a change), bounded by
 * a small iteration cap so a mis-behaving pass cannot loop forever.
 */
class MachinePassManager {
 public:
  explicit MachinePassManager(TargetContext *ctx) : ctx_(ctx) {}
  ~MachinePassManager() {
    for (auto *pass : passes_) {
      delete pass;
    }
  }

  auto AddPass(MachineFunctionPass *pass) -> void { passes_.emplace_back(pass); }
  auto Empty() const -> bool { return passes_.empty(); }

  /// Run every pass over `func`, repeating the whole pipeline until no pass makes
  /// a change (or the iteration cap is hit).
  auto Run(TargetFunction *func) -> void {
    constexpr int MAX_ITERS = 8;
    for (int iter = 0; iter < MAX_ITERS; ++iter) {
      bool changed = false;
      for (auto *pass : passes_) {
        changed |= pass->RunOnFunction(func);
      }
      if (!changed) {
        break;
      }
    }
  }

 private:
  TargetContext *ctx_;
  std::vector<MachineFunctionPass *> passes_;
};

}  // namespace backend::targetir

#endif
