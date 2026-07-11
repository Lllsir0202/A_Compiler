#ifndef BACKEND_TARGETS_UTILS_REGALLOC_GRAPHCOLORING_H
#define BACKEND_TARGETS_UTILS_REGALLOC_GRAPHCOLORING_H

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "backend/targets/utils/regalloc/registerinfo.h"

namespace backend::targetir {
class TargetFunction;
class TargetOperand;

/**
 * @brief Target-independent graph-coloring register allocator
 *        (Iterated Register Coalescing, George & Appel 1996 / Appel ch.11).
 *
 * Operates on a TargetFunction whose instructions still reference virtual
 * registers (plus a few pre-colored physical registers for ABI / fixed-operand
 * constraints). On return every operand is a physical register and any spilled
 * value has been rewritten with load/store around its uses, backed by a frame
 * slot. Integer and floating register classes are colored independently, each
 * against the allocatable set reported by RegisterInfo.
 *
 * Skeleton: the phase methods below are declared to fix the module contract; the
 * bodies are filled in during milestone M2. Nothing links against them yet.
 */
class GraphColoringAllocator {
 public:
  GraphColoringAllocator(TargetFunction *func, const RegisterInfo *reg_info, TargetRegClass reg_class)
      : func_(func), reg_info_(reg_info), reg_class_(reg_class) {}

  /// Allocate one register class to fixpoint. Returns true if the program was
  /// left unchanged (no actual spills => allocation complete for this class).
  auto Run() -> bool;

 private:
  // --- Chaitin/Briggs/George worklist phases ---
  auto LivenessAnalysis() -> void;   // per-block live-in/out via backward dataflow
  auto Build() -> void;              // interference graph + move edges
  auto MakeWorklists() -> void;      // partition nodes: simplify / freeze / spill
  auto Simplify() -> void;          // remove low-degree non-move node, push on stack
  auto Coalesce() -> void;          // conservative move merge (Briggs/George) => kill mv
  auto Freeze() -> void;            // give up coalescing a low-degree move node
  auto SelectSpill() -> void;       // optimistically push a high-degree node
  auto AssignColors() -> void;      // pop stack, pick a color or mark actual spill
  auto RewriteProgram() -> void;    // materialize spills as load/store + frame slots

  /// Spill cost heuristic = (defs+uses weighted by loop depth) / degree.
  auto SpillCost(TargetOperand *vreg) const -> double;

  TargetFunction *func_;
  const RegisterInfo *reg_info_;
  TargetRegClass reg_class_;

  // Interference graph and worklists (populated by the phases above).
  std::unordered_map<TargetOperand *, std::unordered_set<TargetOperand *>> adjacency_;
  std::unordered_map<TargetOperand *, int> degree_;
  std::vector<TargetOperand *> select_stack_;
  std::unordered_set<TargetOperand *> spilled_nodes_;
};

}  // namespace backend::targetir

#endif
