#ifndef BACKEND_TARGETS_AARCH64_REGALLOC_AARCH64ALLOC_H
#define BACKEND_TARGETS_AARCH64_REGALLOC_AARCH64ALLOC_H

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "backend/targets/aarch64/inst/aarch64inst.h"
#include "backend/targets/aarch64/regs/aarch64reginfo.h"
#include "backend/targets/utils/context/targetcontext.h"

namespace backend::targetir {
class TargetFunction;
class TargetBlock;
class TargetInst;
class TargetOperand;
}  // namespace backend::targetir

namespace backend::targetir::aarch64 {

/**
 * @brief Graph-coloring register allocator (Iterated Register Coalescing,
 * George & Appel 1996 / Appel "Modern Compiler Implementation" ch.11).
 *
 * Replaces NaiveRegAlloc behind the same pipeline slot: `Run` rewrites the
 * function so no virtual registers remain, spilling to frame slots (above the
 * outgoing-arg + alloca area at `spill_base`) only when a value cannot be
 * colored. The integer (GPR) and floating (FPR) register files are colored
 * independently, each against the allocatable set reported by AArch64RegisterInfo;
 * coalescing removes ABI / phi copies, and values live across a call are pushed
 * onto callee-saved registers (or spilled) via caller-saved clobber interference.
 *
 * After allocation, `UsedCalleeSaved` reports the callee-saved physical registers
 * handed out, so the frame layer preserves exactly those in the prologue/epilogue.
 * t3 (address scratch) and fp (frame pointer) are reserved and never allocated.
 */
class AArch64GraphColorRegAlloc {
 public:
  explicit AArch64GraphColorRegAlloc(TargetContext *ctx) : ctx_(ctx) {}

  /// Allocate `func`. `spill_base` is the byte offset (from sp) where the spill
  /// area begins. Returns the number of 8-byte spill slots consumed.
  auto Run(TargetFunction *func, size_t spill_base) -> size_t;

  /// Callee-saved (class,id) registers actually assigned, for the frame layer.
  auto UsedCalleeSaved() const -> const std::vector<std::pair<TargetRegClass, uint8_t>> & { return used_callee_saved_; }

 private:
  // A node in the interference graph is a physical or virtual register. Physical
  // registers are canonicalized by (class,id); virtuals by their operand pointer.
  using NodeId = int;

  // --- one full allocation of a single register class, to fixpoint ---
  auto AllocateClass(TargetRegClass reg_class) -> void;
  auto BuildCFG() -> void;
  auto ComputeLoopDepth() -> void;  // per-block loop-nesting depth, for spill weighting
  auto Liveness() -> void;          // per-block live-in/out (backward dataflow)
  auto Build() -> void;             // interference graph + move edges
  auto MakeWorklists() -> void;
  auto Simplify() -> void;
  auto Coalesce() -> void;
  auto Freeze() -> void;
  auto SelectSpill() -> void;
  auto AssignColors() -> void;    // returns via spilled_nodes_ whether a rewrite is needed
  auto RewriteProgram() -> void;  // materialize spills; grows the program, then reiterate
  auto ApplyColors() -> void;     // replace vregs with their physical color, drop nop moves

  // Graph helpers.
  auto AddEdge(NodeId u, NodeId v) -> void;
  auto Adjacent(NodeId n) -> std::vector<NodeId>;
  auto NodeMoves(NodeId n) -> std::vector<TargetInst *>;
  auto MoveRelated(NodeId n) -> bool;
  auto DecrementDegree(NodeId n) -> void;
  auto EnableMoves(const std::vector<NodeId> &nodes) -> void;
  auto AddWorkList(NodeId n) -> void;
  auto Combine(NodeId u, NodeId v) -> void;
  auto GeorgeOK(NodeId t, NodeId r) -> bool;
  auto Conservative(const std::vector<NodeId> &nodes) -> bool;
  auto GetAlias(NodeId n) -> NodeId;
  auto FreezeMoves(NodeId u) -> void;

  // Node bookkeeping.
  auto NodeOf(TargetOperand *op) -> NodeId;         // create/lookup, current class only
  auto IsInClass(TargetOperand *op) const -> bool;  // register of the class being colored
  auto DefsUses(TargetInst *inst, std::vector<NodeId> *defs, std::vector<NodeId> *uses) -> void;
  auto IsMove(TargetInst *inst, NodeId *dst, NodeId *src) -> bool;
  auto SpillCost(NodeId n) const -> double;

  // Spill code.
  auto SlotOf(TargetOperand *vreg) -> size_t;
  auto EmitReload(std::vector<TargetInst *> *out, TargetOperand *vreg, TargetOperand *dst, size_t slot) -> void;
  auto EmitSpill(std::vector<TargetInst *> *out, TargetOperand *vreg, TargetOperand *src, size_t slot) -> void;

  TargetContext *ctx_;
  AArch64RegisterInfo reg_info_;
  TargetFunction *func_ = nullptr;

  // Spill slots (shared numbering across both classes, offset from spill_base_).
  size_t spill_base_ = 0;
  std::unordered_map<TargetOperand *, size_t> spill_slot_;
  size_t spill_count_ = 0;

  std::vector<std::pair<TargetRegClass, uint8_t>> used_callee_saved_;

  // Final assignment: virtual register operand -> physical id (both classes),
  // applied to the program in one pass after both classes are colored.
  std::unordered_map<TargetOperand *, uint8_t> assigned_;

  // --- per-class state (reset in AllocateClass) ---
  TargetRegClass class_{};
  int num_colors_ = 0;
  // Allocatable physical-register ids for the class being colored; a value may be
  // coalesced only into one of these (never into a reserved register like sp).
  std::unordered_set<int> allocatable_ids_;

  std::vector<TargetOperand *> nodes_;  // NodeId -> operand
  std::unordered_map<TargetOperand *, NodeId> vreg_node_;
  std::unordered_map<int, NodeId> phys_node_;  // (class<<8|id) -> node
  std::vector<bool> precolored_;
  std::vector<int> color_;  // -1 = uncolored; else phys id

  std::vector<std::unordered_set<NodeId>> adj_list_;
  std::unordered_set<long long> adj_set_;
  std::vector<int> degree_;
  std::vector<NodeId> alias_;
  std::vector<std::vector<TargetInst *>> move_list_;  // moves touching each node
  std::vector<double> spill_weight_;                  // def+use frequency, for spill choice

  // Temps introduced by spill rewriting: never chosen as spill candidates again
  // (their live ranges are tiny), which guarantees the outer loop terminates.
  std::unordered_set<TargetOperand *> spill_temps_;

  std::unordered_set<NodeId> simplify_worklist_;
  std::unordered_set<NodeId> freeze_worklist_;
  std::unordered_set<NodeId> spill_worklist_;
  std::unordered_set<NodeId> spilled_nodes_;
  std::unordered_set<NodeId> coalesced_nodes_;
  std::unordered_set<NodeId> colored_nodes_;
  std::vector<NodeId> select_stack_;
  std::unordered_set<NodeId> select_set_;

  std::unordered_set<TargetInst *> worklist_moves_;
  std::unordered_set<TargetInst *> active_moves_;
  std::unordered_set<TargetInst *> frozen_moves_;
  std::unordered_set<TargetInst *> constrained_moves_;
  std::unordered_set<TargetInst *> coalesced_moves_;

  // CFG + liveness (rebuilt every Build iteration; program grows on spill).
  std::unordered_map<TargetBlock *, std::vector<TargetBlock *>> succ_;
  std::unordered_map<TargetBlock *, int> loop_depth_;  // nesting depth per block (topology-stable)
  std::unordered_map<TargetBlock *, std::unordered_set<NodeId>> live_in_;
  std::unordered_map<TargetBlock *, std::unordered_set<NodeId>> live_out_;
};

}  // namespace backend::targetir::aarch64

#endif
