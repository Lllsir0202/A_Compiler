#include "targets/utils/ra/liverange/analysis.h"
#include <algorithm>
#include <stack>
#include <unordered_set>
#include "backend/targets/utils/inst/targetinst.h"
#include "backend/targets/utils/operand/targetreg.h"
#include "backend/targets/utils/ra/liverange/liverange.h"
#include "backend/targets/utils/struct/targetblock.h"
#include "backend/targets/utils/struct/targetfunction.h"
#include "context/backend_context.h"
namespace backend::targetir {
// Get the in and out every block
auto LiveRangeAnalysis::GetLiveInOut(TargetFunction *function) -> void {
  // Get the RPO of the blocks
  GetRPO(function);

  // Iterate until the live-in and live-out of each block is stable
  bool changed = true;
  while (changed) {
    changed = false;
    for (auto block : rpo_queue_) {
      auto old_live_in = block_live_in_bitset_[block];
      auto old_live_out = block_live_out_bitset_[block];

      // Calculate the live-out of this block, which is the union of the live-in of its successors
      block_live_out_bitset_[block] = BitSet(function->GetVirtualRegisters().size());
      for (auto succ : block->GetSuccessors()) {
        block_live_out_bitset_[block] |= block_live_in_bitset_[succ];
      }

      // For live-in of the block, it is the union of its use and the live-out minus def
      // live-in = use U (live-out - def),
      // since it should include the use and the live-out that will be used in the successors,
      // but it should not include the def
      // because the def will be defined in this block and will not be used in the successors.
      block_live_in_bitset_[block] = block_use_operands_[block];
      auto tmp = block_live_out_bitset_[block];
      tmp -= block_def_operands_[block];
      block_live_in_bitset_[block] |= tmp;

      // Check if changed
      if (block_live_in_bitset_[block] != old_live_in || block_live_out_bitset_[block] != old_live_out) {
        changed = true;
      }
    }
  }
}

auto LiveRangeAnalysis::AnalyzeLiveRange(TargetFunction *function) -> void {
  // Analyze the live range of each virtual register based on the live-in and live-out of each block
  GetLiveInOut(function);

  // For each block, calculate the live range based on each virtual instruction
  // For use, if it is first time to be used, the live range will be the num.
}

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

auto LiveRangeAnalysis::Prepare(TargetFunction *function) -> void {
  // Calculate the use and def of each block
  for (auto block : function->GetBlocks()) {
    auto use_bitset = BitSet(function->GetVirtualRegisters().size());
    auto def_bitset = BitSet(function->GetVirtualRegisters().size());
    for (auto inst : block->GetInsts()) {
      // All uses will be in the operands, and def will be in the result
      for (auto operand : inst->GetOperands()) {
        if (operand->IsVirtualReg()) {
          auto vreg = static_cast<TargetVirtualReg *>(operand);
          if (!def_bitset.Test(vreg->GetID())) {
            use_bitset.Set(vreg->GetID());
          }
        }
      }
      // Set def bitset
      auto result = inst->GetResult();
      if (result != nullptr && result->IsVirtualReg()) {
        auto vreg = static_cast<TargetVirtualReg *>(result);
        def_bitset.Set(vreg->GetID());
      }
    }
    block_use_operands_[block] = use_bitset;
    block_def_operands_[block] = def_bitset;
  }
}

}  // namespace backend::targetir