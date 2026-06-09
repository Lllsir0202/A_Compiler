#include "backend/targets/utils/ra/liverange/liverange.h"
#include <algorithm>

namespace backend::targetir {
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
    if (new_segments.back().GetEnd() >= segments_[i].GetStart()) {
      // Merge
      new_segments.back() =
          Segment(new_segments.back().GetStart(), std::max(new_segments.back().GetEnd(), segments_[i].GetEnd()));
    } else {
      new_segments.emplace_back(segments_[i]);
    }
  }
  segments_ = std::move(new_segments);
}

}  // namespace backend::targetir