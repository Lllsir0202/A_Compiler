#include "backend/targets/utils/ra/method/graphcoloring/graphcoloring.h"
#include <algorithm>
#include <set>
#include "context/backend_context.h"
#include "targets/utils/operand/targetreg.h"
namespace backend::targetir {
/**
 * @brief Interference graph implementation, which is used for graph coloring register allocation.
 */
auto InterferenceGraph::AddEdge(TargetVirtualReg *vreg1, TargetVirtualReg *vreg2) -> void {
  if (vreg1 == vreg2) {
    return;
  }
  // Regard as undirected graph, add edge in both directions
  adjacency_list_[vreg1].insert(vreg2);
  adjacency_list_[vreg2].insert(vreg1);
}

auto InterferenceGraph::GetDegree(TargetVirtualReg *vreg) -> size_t { return adjacency_list_[vreg].size(); }

auto InterferenceGraph::RecordCopy(TargetVirtualReg *src, TargetVirtualReg *dst) -> void {
  copy_adjacency_list_[dst].insert(src);
  copy_adjacency_list_[src].insert(dst);
}

auto InterferenceGraph::GetCopyNeighbors(TargetVirtualReg *vreg) -> const std::unordered_set<TargetVirtualReg *> & {
  return copy_adjacency_list_[vreg];
}

auto InterferenceGraph::GetNeighbors(TargetVirtualReg *vreg) -> const std::unordered_set<TargetVirtualReg *> & {
  return adjacency_list_[vreg];
}

/**
 * @brief Graph coloring register allocator implementation.
 * Using the interfence graph.
 */
auto GraphColoringAllocator::Allocate() -> void {
  // TODO(loki): Implement the graph coloring register allocation algorithm here
}

auto GraphColoringAllocator::BuildInterferenceGraph(TargetRegClass reg_class, TargetFunction *function) -> void {
  // Sort the live segments by their start point
  // Get the specific function's live ranges
  auto vregs = function->GetVirtualRegisters();
  for (auto vreg : vregs) {
    if (vreg->GetRegClass() != reg_class) {
      continue;
    }
    auto live_range = context_->GetLiveRange(vreg);
    for (auto segment : live_range.GetSegments()) {
      sorted_live_segments_.emplace_back(vreg, segment);
    }
  }

  // Sort by the start point of the segment
  std::sort(sorted_live_segments_.begin(), sorted_live_segments_.end(),
            [](const std::pair<TargetVirtualReg *, Segment> &a, const std::pair<TargetVirtualReg *, Segment> &b) {
              return a.second.GetStart() < b.second.GetStart();
            });

  // Build the interference graph by active set
  std::set<std::pair<TargetVirtualReg *, Segment>> active_set;
  for (const auto &[vreg, segment] : sorted_live_segments_) {
    // Remove expired segments from the active set
    for (auto it = active_set.begin(); it != active_set.end();) {
      if (it->second.GetEnd() <= segment.GetStart()) {
        it = active_set.erase(it);
      } else {
        ++it;
      }
    }

    // Add interference edges between the current segment and all active segments
    for (const auto &[active_vreg, active_segment] : active_set) {
      // Add an edge between vreg and active_vreg in the interference graph
      if (reg_class == TargetRegClass::General) {
        gpr_interference_graph_.AddEdge(vreg, active_vreg);
      } else if (reg_class == TargetRegClass::Float) {
        fpr_interference_graph_.AddEdge(vreg, active_vreg);
      } else if (reg_class == TargetRegClass::Vector) {
        vector_interference_graph_.AddEdge(vreg, active_vreg);
      }
    }

    // Add the current segment to the active set
    active_set.insert({vreg, segment});
  }
}

}  // namespace backend::targetir