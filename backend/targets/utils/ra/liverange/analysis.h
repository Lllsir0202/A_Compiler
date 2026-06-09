#ifndef BACKEND_TARGETS_UTILS_RA_LIVERANGE_LIVERANGE_H
#define BACKEND_TARGETS_UTILS_RA_LIVERANGE_LIVERANGE_H

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
  auto GetLiveInOut() -> void;
  auto AnalyzeLiveRange() -> void;
};

}  // namespace backend::targetir
#endif