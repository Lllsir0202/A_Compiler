#ifndef BACKEND_TARGETS_UTILS_RA_METHOD_GRAPHCOLORING_GRAPHCOLORING_H
#define BACKEND_TARGETS_UTILS_RA_METHOD_GRAPHCOLORING_GRAPHCOLORING_H

#include <unordered_map>
#include <unordered_set>
#include "backend/targets/utils/operand/targetreg.h"
#include "backend/targets/utils/ra/liverange/liverange.h"
#include "backend/targets/utils/ra/method/allocator.h"
#include "targets/utils/operand/targetoperand.h"
#include "targets/utils/ra/method/costmodel/costmodel.h"
#include "targets/utils/struct/targetfunction.h"

namespace context {
class BackendContext;
}

namespace backend::targetir {
// Interference graph
class InterferenceGraph {
 public:
  // Add an edge between two virtual registers in the interference graph
  auto AddEdge(TargetVirtualReg *vreg1, TargetVirtualReg *vreg2) -> void;

  // Get the degree of a virtual register in the interference graph
  auto GetDegree(TargetVirtualReg *vreg) -> size_t;

  // Remove a specified virtual register from the interference graph,
  // which is used for the simplify phase in graph coloring.
  auto RemoveNode(TargetVirtualReg *vreg) -> void;

  // Record all copy instructions for coalescing,
  // which means to try to assign the same color to the source and destination
  // virtual registers of a copy instruction if possible.
  auto RecordCopy(TargetVirtualReg *src, TargetVirtualReg *dst) -> void;

  // Get the copy neighbors of a virtual register in the copy graph
  auto GetCopyNeighbors(TargetVirtualReg *vreg) -> const std::unordered_set<TargetVirtualReg *> &;

  // Get the neighbors of a virtual register in the interference graph
  auto GetNeighbors(TargetVirtualReg *vreg) -> const std::unordered_set<TargetVirtualReg *> &;

  // Get the graph adjacency list.
  auto GetAdjacencyList() -> const std::unordered_map<TargetVirtualReg *, std::unordered_set<TargetVirtualReg *>> & {
    return adjacency_list_;
  }

  auto GetAllVirtualRegs() -> std::unordered_set<TargetVirtualReg *> {
    std::unordered_set<TargetVirtualReg *> vregs;
    for (const auto &[vreg, neighbors] : adjacency_list_) {
      vregs.insert(vreg);
    }
    return vregs;
  }

 private:
  // Adjacency list representation of the interference graph
  std::unordered_map<TargetVirtualReg *, std::unordered_set<TargetVirtualReg *>> adjacency_list_;

  // Copy graph
  // the destination would prefer to be assigned to the same as source if possible.
  std::unordered_map<TargetVirtualReg *, std::unordered_set<TargetVirtualReg *>> copy_adjacency_list_;
};

class GraphColoringAllocator : public RegisterAllocator {
 public:
  ~GraphColoringAllocator() override = default;

  auto Allocate() -> void override;

 private:
  auto Build(TargetFunction *function) -> void;

  auto BuildInterferenceGraph(TargetRegClass reg_class, TargetFunction *function) -> void;

  // Based on the interference graph, assign colors, and handle the spill if necessary.
  auto Color() -> void;

  auto ColorRegClass(TargetRegClass reg_class) -> void;

  // Simplify the interference graph by removing nodes
  // with degree less than the number of available colors (registers).
  auto Simplify(InterferenceGraph &interference_graph) -> void;

  // Select a color for the removed node, and assign it to the corresponding virtual register.
  auto Select(InterferenceGraph &interference_graph) -> void;

  std::vector<std::pair<TargetVirtualReg *, Segment>> sorted_live_segments_;

  // Backend context
  context::BackendContext *context_;

  // Interference graph
  InterferenceGraph gpr_interference_graph_;
  InterferenceGraph fpr_interference_graph_;
  InterferenceGraph vector_interference_graph_;

  // Stack for the simplify phase, which records the removed nodes in order.
  std::vector<TargetVirtualReg *> simplify_stack_;

  // Set of candidated spilled virtual registers,
  // which are those that cannot be colored with the available colors (registers).
  std::unordered_set<TargetVirtualReg *> spilled_vregs_;

  // Cost model for calculating the cost and priority of coloring or spilling a virtual register,
  // which is used for the simplify and select phase.
  CostModel cost_model_;
};

}  // namespace backend::targetir

#endif