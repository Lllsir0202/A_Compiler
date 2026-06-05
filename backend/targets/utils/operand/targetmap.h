#ifndef BACKEND_TARGETS_UTILS_OPERAND_TARGETMAP_H
#define BACKEND_TARGETS_UTILS_OPERAND_TARGETMAP_H

#include "targets/utils/operand/targetoperand.h"
namespace backend::targetir {
// Used as a map from num and size to decide what to print.
// Inherit from this class to implement the map for different architectures, e.g. x86-64, arm64, etc.
class TargetMap {
 public:
  virtual auto GetRegName(TargetRegSize reg_size, uint8_t id) -> std::string = 0;
  virtual ~TargetMap() = default;
};

}  // namespace backend::targetir

#endif