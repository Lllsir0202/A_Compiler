#ifndef BACKEND_TARGETS_UTILS_RA_LIVERANGE_ANALYSIS_H
#define BACKEND_TARGETS_UTILS_RA_LIVERANGE_ANALYSIS_H

#include <bitset>
#include <queue>
#include "backend/targets/utils/struct/targetfunction.h"

namespace context {
class BackendContext;
}

namespace backend::targetir {
class LiveRangeAnalysis {
 public:
  auto Analyze() -> void;

 private:
  /**
   * @brief Need to run following steps to get the live range of each virtual register:
   * 1. Get the live-in and live-out operands of each block
   * 2. Analyze the live range of each virtual register
   */
  auto GetLiveInOut(TargetFunction *function) -> void;
  auto AnalyzeLiveRange(TargetFunction *functionu) -> void;

  // Go through all blocks with RPO(Reverse Post Order) to get the live range of each virtual register
  auto GetRPO(TargetFunction *function) -> void;

  // RPO
  std::deque<backend::targetir::TargetBlock *> rpo_queue_;
  // Get Context
  context::BackendContext *context_;

  // BitSet to record the live in and live out of each block
  
};

}  // namespace backend::targetir
#endif