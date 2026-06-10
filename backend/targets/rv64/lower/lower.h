#ifndef BACKEND_TARGETS_RV64_LOWER_LOWER_H
#define BACKEND_TARGETS_RV64_LOWER_LOWER_H

#include "backend/targets/utils/struct/targetmodule.h"

namespace backend::targetir::rv64 {
class RV64Lower {
 public:
  auto Lower(TargetModule *module) -> void;
  ~RV64Lower() = default;

 private:
};
}  // namespace backend::targetir::rv64

#endif