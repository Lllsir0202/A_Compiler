#include "backend/targets/utils/ra/liverange/liverange.h"
#include <algorithm>

namespace backend::targetir {
static auto Overlap(const Segment &a, const Segment &b) -> bool {
  // Since liverange is [start, end), so it overlaps
  // if a.start < b.end && b.start < a.end, with no equality.
  return a.GetStart() < b.GetEnd() && b.GetStart() < a.GetEnd();
}

auto LiveRange::AddSegment(size_t start, size_t end) -> void { segments_.emplace_back(start, end); }

auto LiveRange::Canalize() -> void {
  if (segments_.empty()) {
    return;
  }
  // Sort the segments by start
  std::sort(segments_.begin(), segments_.end(),
            [](const Segment &a, const Segment &b) { return a.GetStart() < b.GetStart(); });
  // Merge the overlapping segments
  std::vector<Segment> new_segments;
  new_segments.reserve(segments_.size());
  new_segments.emplace_back(segments_[0]);

  // From back to front,
  // if the current segment overlaps with the last segment in new_segments,
  // merge them.
  for (size_t i = 1; i < segments_.size(); ++i) {
    // If overlap, merge the current segment with the last segment in new_segments
    if (segments_[i].GetStart() <= new_segments.back().GetEnd()) {
      new_segments.back().SetEnd(std::max(new_segments.back().GetEnd(), segments_[i].GetEnd()));
    } else {
      // Otherwise, add the current segment to new_segments
      new_segments.emplace_back(segments_[i]);
    }
  }
  segments_ = std::move(new_segments);
}

// Check whether two live ranges overlap
auto LiveRange::OverlapsWith(const LiveRange &other) const -> bool {
  size_t i = 0;
  size_t j = 0;
  while (i < segments_.size() && j < other.segments_.size()) {
    if (Overlap(segments_[i], other.segments_[j])) {
      return true;
    }
    if (segments_[i].GetStart() < other.segments_[j].GetStart()) {
      ++i;
    } else {
      ++j;
    }
  }
  return false;
}

auto LiveRange::IsActiveAt(size_t inst_num) const -> bool {
  return std::any_of(segments_.begin(), segments_.end(), [inst_num](const Segment &segment) {
    return segment.GetStart() <= inst_num && inst_num < segment.GetEnd();
  });
}

// If it is not active, it must be expired, since all segments do not overlap this instruction number.
auto LiveRange::IsExpiredAt(size_t inst_num) const -> bool { return !IsActiveAt(inst_num); }

}  // namespace backend::targetir