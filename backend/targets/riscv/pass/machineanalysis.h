#ifndef BACKEND_TARGETS_RISCV_PASS_MACHINEANALYSIS_H
#define BACKEND_TARGETS_RISCV_PASS_MACHINEANALYSIS_H

#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "backend/targets/utils/operand/targetoperand.h"

namespace backend::targetir {
class TargetBlock;
class TargetFunction;
class TargetInst;
}  // namespace backend::targetir

namespace backend::targetir::riscv {

struct MachineNaturalLoop {
  TargetBlock *header_ = nullptr;
  TargetBlock *preheader_ = nullptr;
  std::unordered_set<TargetBlock *> blocks_;
};

/**
 * Canonical CFG and dominance analysis for selected RISC-V machine IR.
 *
 * Branch targets are read from the opcode descriptor and an implicit
 * fallthrough is added only when the final instruction does not unconditionally
 * transfer control. Natural loops with the same header are merged, matching the
 * representation expected by LICM and spill weighting.
 */
class MachineCFG {
 public:
  explicit MachineCFG(TargetFunction *function);

  auto Blocks() const -> const std::vector<TargetBlock *> &;
  auto Successors(TargetBlock *block) const -> const std::vector<TargetBlock *> &;
  auto Predecessors(TargetBlock *block) const -> const std::vector<TargetBlock *> &;
  auto BlockForLabel(const std::string &label) const -> TargetBlock *;
  auto ContainsLabel(const std::string &label) const -> bool;
  auto Dominates(TargetBlock *dominator, TargetBlock *block) const -> bool;
  auto DominatorChildren(TargetBlock *block) const -> const std::vector<TargetBlock *> &;
  auto NaturalLoops() const -> const std::vector<MachineNaturalLoop> & { return loops_; }
  auto LoopDepth(TargetBlock *block) const -> int;

 private:
  auto BuildEdges() -> void;
  auto ComputeDominators() -> void;
  auto FindNaturalLoops() -> void;

  TargetFunction *function_;
  std::unordered_map<std::string, TargetBlock *> by_label_;
  std::unordered_map<TargetBlock *, size_t> index_;
  std::unordered_map<TargetBlock *, std::vector<TargetBlock *>> successors_;
  std::unordered_map<TargetBlock *, std::vector<TargetBlock *>> predecessors_;
  std::vector<std::unordered_set<size_t>> dominators_;
  std::unordered_map<TargetBlock *, std::vector<TargetBlock *>> dom_children_;
  std::vector<MachineNaturalLoop> loops_;
  std::unordered_map<TargetBlock *, int> loop_depth_;
};

/**
 * Backward virtual-register liveness with machine-SSA Phi semantics.
 *
 * A Phi definition belongs to its destination block, while each incoming value
 * is used on the corresponding predecessor edge. Treating those values as
 * ordinary header uses makes unrelated loop values spuriously live around the
 * whole loop and distorts both LICM pressure and future spill decisions.
 */
class MachineLiveness {
 public:
  using RegSet = std::unordered_set<TargetOperand *>;

  MachineLiveness(TargetFunction *function, const MachineCFG &cfg);

  auto LiveIn(TargetBlock *block) const -> const RegSet &;
  auto LiveOut(TargetBlock *block) const -> const RegSet &;
  auto EdgeUses(TargetBlock *predecessor, TargetBlock *successor) const -> const RegSet &;
  auto PeakPressure(const std::unordered_set<TargetBlock *> &blocks, TargetRegClass reg_class) const -> int;

 private:
  using Edge = std::pair<TargetBlock *, TargetBlock *>;

  struct EdgeHash {
    auto operator()(const Edge &edge) const -> size_t;
  };

  auto BuildLocalSets() -> void;
  auto Solve() -> void;
  static auto IsVirtualClass(TargetOperand *operand, TargetRegClass reg_class) -> bool;

  TargetFunction *function_;
  const MachineCFG &cfg_;
  std::unordered_map<TargetBlock *, RegSet> uses_;
  std::unordered_map<TargetBlock *, RegSet> defs_;
  std::unordered_map<Edge, RegSet, EdgeHash> edge_uses_;
  std::unordered_map<TargetBlock *, RegSet> live_in_;
  std::unordered_map<TargetBlock *, RegSet> live_out_;
};

}  // namespace backend::targetir::riscv

#endif
