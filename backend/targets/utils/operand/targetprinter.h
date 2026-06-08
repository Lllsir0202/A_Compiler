#ifndef BACKEND_TARGETS_UTILS_OPERAND_TARGETMAP_H
#define BACKEND_TARGETS_UTILS_OPERAND_TARGETMAP_H

#include "targets/utils/operand/targetoperand.h"
namespace backend::targetir {
// Used as a map from num and size to decide what to print.
// Inherit from this class to implement the map for different architectures, e.g. x86-64, arm64, etc.
class TargetRegPrinter {
 public:
  virtual auto GetRegName(TargetRegSize reg_size, uint8_t id, TargetRegClass reg_class) const -> std::string = 0;
  virtual ~TargetRegPrinter() = default;
};

// Print the symbol based on the architecture.
class TargetSymbolPrinter {
 public:
  virtual auto GetSymbolName(const std::string &name, TargetSymbolAccessMode access_mode) const -> std::string = 0;
  virtual ~TargetSymbolPrinter() = default;
};

}  // namespace backend::targetir

#endif