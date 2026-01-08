#ifndef STAGES_STAGE_H
#define STAGES_STAGE_H

namespace stages {
// We define a adapter interface for different stages
class Stage {
 public:
  virtual ~Stage() = default;
  virtual auto Run() -> void = 0;
};

}  // namespace stages

#endif