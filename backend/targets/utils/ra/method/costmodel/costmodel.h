#ifndef BACKEND_TARGETS_UTILS_RA_METHOD_COSTMODEL_COSTMODEL_H
#define BACKEND_TARGETS_UTILS_RA_METHOD_COSTMODEL_COSTMODEL_H

#include "backend/targets/utils/operand/targetreg.h"

namespace backend::targetir {
class CostModel {
 public:
  ~CostModel() = default;

  // Calculate the cost and priority of dealing with a virtual register
  auto CalculateCost(TargetVirtualReg *vreg) -> double;

  auto CalculatePriority(TargetVirtualReg *vreg) -> double;
};

}  // namespace backend::targetir

#endif