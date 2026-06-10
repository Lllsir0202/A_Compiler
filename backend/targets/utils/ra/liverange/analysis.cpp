#include "targets/utils/ra/liverange/analysis.h"
#include <algorithm>
#include <stack>
#include "backend/targets/utils/ra/liverange/liverange.h"
#include "backend/targets/utils/struct/targetblock.h"
#include "backend/targets/utils/struct/targetfunction.h"
#include "context/backend_context.h"
namespace backend::targetir {
// Get the in and out every block
auto LiveRangeAnalysis::GetLiveInOut(TargetFunction *function) -> void {
 
}

auto LiveRangeAnalysis::AnalyzeLiveRange(TargetFunction *function) -> void {}

auto LiveRangeAnalysis::GetRPO(TargetFunction *function) -> void {
  // Reset the RPO queue
  rpo_queue_.clear();

  auto blocks = function->GetBlocks();
  auto entry_block = blocks[0];
  // Get the RPO of the blocks
  std::vector<bool> visited(blocks.size(), false);
  std::stack<TargetBlock *> stack;
  stack.push(entry_block);
  while (!stack.empty()) {
    auto block = stack.top();
    stack.pop();
    if (visited[block->GetID()]) {
      continue;
    }
    visited[block->GetID()] = true;
    rpo_queue_.push_back(block);
    for (auto succ : block->GetSuccessors()) {
      if (!visited[succ->GetID()]) {
        stack.push(succ);
      }
    }
  }

  // Reverse the RPO queue
  std::reverse(rpo_queue_.begin(), rpo_queue_.end());
}

}  // namespace backend::targetir