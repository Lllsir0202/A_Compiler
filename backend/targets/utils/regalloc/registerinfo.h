#ifndef BACKEND_TARGETS_UTILS_REGALLOC_REGISTERINFO_H
#define BACKEND_TARGETS_UTILS_REGALLOC_REGISTERINFO_H

#include <cstdint>
#include <vector>
#include "backend/targets/utils/operand/targetoperand.h"

namespace backend::targetir {

/**
 * @brief Target-independent description of a physical register file, consumed by
 * the graph-coloring allocator so the allocator itself stays target-agnostic.
 *
 * Each concrete target (RISC-V, AArch64, ...) implements this once; the allocator
 * queries it for the allocatable set and calling-convention properties. The
 * integer and floating files are described independently — the allocator colors
 * each register class against its own set.
 */
class RegisterInfo {
 public:
  virtual ~RegisterInfo() = default;

  /// Physical register ids that the allocator may assign for the given class.
  virtual auto AllocatableRegs(TargetRegClass reg_class) const -> const std::vector<uint8_t> & = 0;

  /// The number of colors (k) available for this class = AllocatableRegs().size().
  virtual auto NumAllocatable(TargetRegClass reg_class) const -> size_t = 0;

  /// True if the physical register is clobbered across a call (caller-saved), so
  /// values live across a call must avoid it or be spilled.
  virtual auto IsCallerSaved(TargetRegClass reg_class, uint8_t id) const -> bool = 0;

  /// Registers that must be preserved by the callee (saved in prologue / restored
  /// in epilogue) when the allocator hands them out.
  virtual auto CalleeSavedRegs(TargetRegClass reg_class) const -> const std::vector<uint8_t> & = 0;
};

}  // namespace backend::targetir

#endif
