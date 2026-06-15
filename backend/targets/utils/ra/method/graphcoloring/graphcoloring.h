#ifndef BACKEND_TARGETS_UTILS_RA_METHOD_GRAPHCOLORING_GRAPHCOLORING_H
#define BACKEND_TARGETS_UTILS_RA_METHOD_GRAPHCOLORING_GRAPHCOLORING_H

#include <unordered_map>
#include <unordered_set>
#include "backend/targets/utils/operand/targetreg.h"
#include "backend/targets/utils/ra/method/allocator.h"
namespace backend::targetir {
// Define the interference graph and the graph coloring register allocator here
class InferenceGraph {
 public:
  ~InferenceGraph() = default;

  // Add a node to the graph, which represents a virtual register
  auto AddNode(backend::targetir::TargetVirtualReg *vreg) -> void;

  // Add an edge to the graph, which represents the interference between two virtual registers
  auto AddEdge(backend::targetir::TargetVirtualReg *vreg1, backend::targetir::TargetVirtualReg *vreg2) -> void;

  // Get the neighbors of a node, which are the virtual registers that interfere with this virtual register
  auto GetNeighbors(backend::targetir::TargetVirtualReg *vreg)
      -> const std::unordered_set<backend::targetir::TargetVirtualReg *> &;

 private:
  // The adjacency list representation of the graph, mapping from a virtual register to its neighbors
  std::unordered_map<backend::targetir::TargetVirtualReg *, std::unordered_set<backend::targetir::TargetVirtualReg *>>
      adjacency_list_;
};

class GraphColoringAllocator : public RegisterAllocator {
 public:
  ~GraphColoringAllocator() override = default;

  auto Allocate() -> void override;

 private:
  // The interference graph
  InferenceGraph interference_graph_;
};

}  // namespace backend::targetir

#endif