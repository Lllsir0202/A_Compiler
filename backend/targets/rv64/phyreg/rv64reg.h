#ifndef BACKEND_TARGETS_RV64_PHYREG_RV64REG_H
#define BACKEND_TARGETS_RV64_PHYREG_RV64REG_H

#include "backend/targets/utils/operand/targetprinter.h"
namespace backend::targetir {
class RV64RegPrinter : public TargetRegPrinter {
 public:
  auto GetRegName(TargetRegSize reg_size, uint8_t id, TargetRegClass reg_class,
                          TargetRegRole role) const -> std::string override;
  static auto GetInstance() -> const RV64RegPrinter & {
    static RV64RegPrinter instance;
    return instance;
  }
};
}  // namespace backend::targetir
#endif