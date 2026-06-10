#ifndef BACKEND_TARGETS_UTILS_RA_LIVERANGE_LIVERANGE_H
#define BACKEND_TARGETS_UTILS_RA_LIVERANGE_LIVERANGE_H

/**
 * @brief This file defines the live_range-related classes,
 * which will be used for register allocation.
 */
#include <cstddef>
#include <vector>
#include "targets/utils/struct/targetblock.h"
namespace backend::targetir {

/**
 * @brief Segment represents a segment of a live range
 */
class Segment {
 public:
  Segment(size_t start, size_t end) : start_(start), end_(end) {}
  Segment() = default;
  auto GetStart() const -> size_t { return start_; }
  auto GetEnd() const -> size_t { return end_; }
  auto IsSet() const -> bool { return is_set_; }
  auto SetIsSet(bool is_set) -> void { is_set_ = is_set; }

  // Update the end of this segment, which is used for the first time to be used.
  auto SetEnd(size_t end) -> void { end_ = end; }
  auto SetStart(size_t start) -> void { start_ = start; }

  auto SetBlock(TargetBlock *block) -> void { block_ = block; }
  auto GetBlock() const -> TargetBlock * { return block_; }

 private:
  // Record the start and end instruction number of this segment,
  // which means: [start_, end_), includes the start instruction,
  // but not the end instruction.
  size_t start_;
  size_t end_;

  // Record whether this segment has been set or not, which is used for the first time to be used.
  bool is_set_ = false;

  // Segment in which block
  TargetBlock *block_;
};

// Represents a live range of a virtual register,
// which consists of several segments
/**
 * @note The segments must be non-overlapping
 * and sorted by start before actual usage.
 */
class LiveRange {
 public:
  // Add a segment to this live range.
  auto AddSegment(size_t start, size_t end, TargetBlock *block) -> void;

  auto AddSegment(const Segment &segment) -> void { segments_.emplace_back(segment); }

  // Canalize this live range, which means to make sure
  // all these segments are non-overlapping and sorted by start.
  auto Canalize() -> void;

 private:
  // The segments of this live range, which are non-overlapping and sorted by start instruction number
  std::vector<Segment> segments_;

  // MAYBEUNUSED: The used instructions' number,
  // as well as the definition instruction's number
  std::vector<size_t> used_insts_;

  // TODO(loki): More information added in the future
};

}  // namespace backend::targetir

#endif