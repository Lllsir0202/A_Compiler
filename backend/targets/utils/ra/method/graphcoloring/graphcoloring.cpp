#include "backend/targets/utils/ra/method/graphcoloring/graphcoloring.h"
#include <algorithm>
#include <set>
#include "context/backend_context.h"
#include "targets/utils/operand/targetoperand.h"
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

auto InterferenceGraph::RemoveNode(TargetVirtualReg *vreg) -> void {
  // Remove this node from the adjacency list of its neighbors
  for (auto neighbor : adjacency_list_[vreg]) {
    adjacency_list_[neighbor].erase(vreg);
  }
  // Remove this node from the graph
  adjacency_list_.erase(vreg);
  copy_adjacency_list_.erase(vreg);
}

/**
 * @brief Graph coloring register allocator implementation.
 * Using the interfence graph.
 */
auto GraphColoringAllocator::Allocate() -> void {
  // TODO(loki): Implement the graph coloring register allocation algorithm here
  for (auto function : context_->GetFunctions()) {
    Build(function);
    // TODO(loki): Implement the actual graph coloring algorithm here, e.g., simplify, select, spill, coalesce, etc.
  }
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

auto GraphColoringAllocator::Build(TargetFunction *function) -> void {
  // Build the interference graph for general-purpose registers
  BuildInterferenceGraph(TargetRegClass::General, function);

  // Build the interference graph for floating-point registers
  BuildInterferenceGraph(TargetRegClass::Float, function);

  // Build the interference graph for vector registers
  BuildInterferenceGraph(TargetRegClass::Vector, function);
}

auto GraphColoringAllocator::Color() -> void {
  // Color general-purpose registers
  ColorRegClass(TargetRegClass::General);

  // Color floating-point registers
  ColorRegClass(TargetRegClass::Float);

  // Color vector registers
  ColorRegClass(TargetRegClass::Vector);
}

auto GraphColoringAllocator::ColorRegClass(TargetRegClass reg_class) -> void {
  InterferenceGraph &interference_graph = (reg_class == TargetRegClass::General) ? gpr_interference_graph_
                                          : (reg_class == TargetRegClass::Float) ? fpr_interference_graph_
                                                                                 : vector_interference_graph_;
  Simplify(interference_graph);

  Select(interference_graph);
}

auto GraphColoringAllocator::Simplify(InterferenceGraph &interference_graph) -> void {
  // TODO(loki): Implement the simplify phase of the graph coloring algorithm here
  // Put high priority nodes into a stack first,
  // which means those nodes are more likely to be colored successfully,
  // and less likely to be spilled.
  auto vregs = interference_graph.GetAllVirtualRegs();
  std::unordered_map<TargetVirtualReg *, double> priority_map;
  for (auto vreg : vregs) {
    priority_map[vreg] = cost_model_.CalculatePriority(vreg);
  }

  // Sort the virtual registers by their priority in ascending order
  // which means those with higher priority will be put into the stack later, and colored first.
  std::vector<TargetVirtualReg *> sorted_vregs(vregs.begin(), vregs.end());
  std::sort(sorted_vregs.begin(), sorted_vregs.end(),
            [&priority_map](TargetVirtualReg *a, TargetVirtualReg *b) { return priority_map[a] < priority_map[b]; });

  // Simplify the graph by remove the nodes with degree less than the number of available colors (registers).
  // They are necessarily colored successfully, and won't be spilled.
  for (auto vreg : sorted_vregs) {
    simplify_stack_.push_back(vreg);
    interference_graph.RemoveNode(vreg);
  }
}

auto GraphColoringAllocator::Select(InterferenceGraph &interference_graph) -> void {
  // Pop nodes from the stack, and assign colors to them.
  while (!simplify_stack_.empty()) {
    auto vreg = simplify_stack_.back();
    simplify_stack_.pop_back();

    // Get the colors used by the neighbors of this node
    std::set<PhysicalReg *> used_colors;
    for (auto neighbor : interference_graph.GetNeighbors(vreg)) {
      if (context_->IsMapped(neighbor)) {
        used_colors.insert(context_->GetPregOfVreg(neighbor));
      }
    }

    // Get the available colors for this node, which are those that are not used by its neighbors.
    auto free_regs = context_->GetRAManager().GetFreeRegs(vreg->GetRegClass());
    std::set<PhysicalReg *> available_colors;
    for (auto reg : free_regs) {
      if (used_colors.count(reg) == 0) {
        available_colors.insert(reg);
      }
    }

    if (!available_colors.empty()) {
      if (context_->HasAllocatorHint(vreg)) {
        auto preferred_color = context_->GetAllocatorHint(vreg).GetPreferredColor();
        if (preferred_color != nullptr && available_colors.count(preferred_color) > 0) {
          available_colors = {preferred_color};
        }
      }
      auto assigned_color = *available_colors.begin();
      context_->MapVregToPreg(vreg, assigned_color);
      context_->GetRAManager().MarkRegUsed(vreg->GetRegClass(), assigned_color);
    } else {
      // This node cannot be colored successfully with the available colors (registers),
      // and is spilled to memory.
      spilled_vregs_.insert(vreg);
    }
  }
}

}  // namespace backend::targetir