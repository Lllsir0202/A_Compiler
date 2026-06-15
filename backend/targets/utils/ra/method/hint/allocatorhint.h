#ifndef BACKEND_TARGETS_UTILS_RA_METHOD_HINT_ALLOCATORHINT_H
#define BACKEND_TARGETS_UTILS_RA_METHOD_HINT_ALLOCATORHINT_H

#include "backend/targets/utils/operand/targetreg.h"

namespace backend::targetir {
class AllocatorHint {
 public:
  AllocatorHint() = default;
  ~AllocatorHint() = default;

  auto SetPreferredColor(PhysicalReg *reg) -> void { preferred_color_ = reg; }
  auto GetPreferredColor() -> PhysicalReg * { return preferred_color_; }
  auto HasPreferredColor() -> bool { return preferred_color_ != nullptr; }

 private:
  // Preferred color for this virtual register, which is used for coalescing and other optimizations.
  PhysicalReg *preferred_color_ = nullptr;
};

}  // namespace backend::targetir

#endif