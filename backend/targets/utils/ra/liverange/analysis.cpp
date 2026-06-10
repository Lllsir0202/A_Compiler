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
  Prepare(function);
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
  // For def, if it is the latest time to be defined, the live range will be the num.
  for (auto block : function->GetBlocks()) {
    std::unordered_map<TargetVirtualReg *, Segment> open_segments;
    auto live_out = block_live_out_bitset_[block];
    // For all live-out virtual registers,
    // add a segment from the beginning of this block to the end of this block
    for (auto vreg_id : live_out.GetSetBits()) {
      auto vreg = function->GetVirtualRegisterByID(vreg_id);
      open_segments[vreg] = Segment(0, block->GetInsts().size());
    }

    for (int i = block->GetInsts().size() - 1; i >= 0; --i) {
      auto inst = block->GetInsts()[i];
      // For def, if it is the latest time to be defined, the live range will be the num.
      auto result = inst->GetResult();
      if (result != nullptr && result->IsVirtualReg()) {
        auto vreg = static_cast<TargetVirtualReg *>(result);
        if (open_segments.find(vreg) == open_segments.end()) {
          open_segments[vreg] = Segment(inst->GetInstNum(), inst->GetInstNum() + 1);
        } else {
          // If live out this vreg, it means we have found the real live range of this vreg
          open_segments[vreg].SetStart(inst->GetInstNum());
        }
        // Erase this vreg from open_segments
        auto segment = open_segments[vreg];
        context_->UpdateLiveRange(vreg, segment);
        open_segments.erase(vreg);
      }

      // For use, if it is first time to be used, the live range will be the num.
      for (auto operand : inst->GetOperands()) {
        if (operand->IsVirtualReg()) {
          auto vreg = static_cast<TargetVirtualReg *>(operand);
          if (open_segments.find(vreg) == open_segments.end()) {
            open_segments[vreg] = Segment(inst->GetInstNum(), inst->GetInstNum() + 1);
          } else {
            // If live out this vreg, it means we have found the real live range of this vreg
            open_segments[vreg].SetEnd(inst->GetInstNum());
          }
        }
      }
    }

    // Other open segments are live in this block
    for (auto &[vreg, segment] : open_segments) {
      context_->UpdateLiveRange(vreg, segment);
    }
  }
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

auto LiveRangeAnalysis::Analyze() -> void {
  // Analyze the live range of each virtual register based on the live-in and live-out of each block
  for (auto function : context_->GetModule()->GetFunctions()) {
    AnalyzeLiveRange(function);
  }
}

}  // namespace backend::targetir