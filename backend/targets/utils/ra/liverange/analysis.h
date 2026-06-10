#ifndef BACKEND_TARGETS_UTILS_RA_LIVERANGE_ANALYSIS_H
#define BACKEND_TARGETS_UTILS_RA_LIVERANGE_ANALYSIS_H

#include <deque>
#include <unordered_map>
#include "backend/targets/utils/struct/targetfunction.h"
#include "targets/utils/bitset/bitset.h"

namespace context {
class BackendContext;
}

namespace backend::targetir {
class LiveRangeAnalysis {
 public:
  auto Analyze() -> void;
  ~LiveRangeAnalysis() = default;

 private:
  /**
   * @brief Need to run following steps to get the live range of each virtual register:
   * 1. Get the live-in and live-out operands of each block
   * 2. Analyze the live range of each virtual register
   */
  auto Prepare(TargetFunction *function) -> void;
  auto GetLiveInOut(TargetFunction *function) -> void;
  auto AnalyzeLiveRange(TargetFunction *function) -> void;

  // Go through all blocks with RPO(Reverse Post Order) to get the live range of each virtual register
  auto GetRPO(TargetFunction *function) -> void;

  // RPO
  std::deque<backend::targetir::TargetBlock *> rpo_queue_;
  // Get Context
  context::BackendContext *context_;

  // Map from targetblock* to its live-in and live-out BitSet*
  std::unordered_map<TargetBlock *, BitSet> block_live_in_bitset_;
  std::unordered_map<TargetBlock *, BitSet> block_live_out_bitset_;

  // Record all use and def each block for better calculating the live-in and live-out
  std::unordered_map<TargetBlock *, BitSet> block_use_operands_;
  std::unordered_map<TargetBlock *, BitSet> block_def_operands_;
};

}  // namespace backend::targetir
#endif