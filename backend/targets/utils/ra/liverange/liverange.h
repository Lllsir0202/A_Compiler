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

  // Update the end of this segment, which is used for the first time to be used.
  auto SetEnd(size_t end) -> void { end_ = end; }
  auto SetStart(size_t start) -> void { start_ = start; }

 private:
  // Record the start and end instruction number of this segment,
  // which means: [start_, end_), includes the start instruction,
  // but not the end instruction.
  size_t start_;
  size_t end_;
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
  auto AddSegment(size_t start, size_t end) -> void;

  auto AddSegment(const Segment &segment) -> void { segments_.emplace_back(segment); }

  // Add a usage instruction number to this live range, which is used for the cost model in register allocation.
  auto AddUsedInst(size_t inst_num) -> void { used_insts_.emplace_back(inst_num); }

  // Canalize this live range, which means to make sure
  // all these segments are non-overlapping and sorted by start.
  auto Canalize() -> void;

  // Check whether two live ranges overlap
  auto OverlapsWith(const LiveRange &other) const -> bool;

  // Check whether this live range active in a specific instruction number
  auto IsActiveAt(size_t inst_num) const -> bool;

  // Check whether this live range is expired in a specific instruction number
  auto IsExpiredAt(size_t inst_num) const -> bool;

  // Get the usage frequency of this live range, which is the number of usage instructions.
  auto GetUsageFrequency() const -> size_t { return used_insts_.size(); }

 private:
  // The segments of this live range, which are non-overlapping and sorted by start instruction number
  std::vector<Segment> segments_;

  // MAYBEUNUSED: The used instructions' number
  std::vector<size_t> used_insts_;

  // TODO(loki): More information added in the future
};

}  // namespace backend::targetir

#endif