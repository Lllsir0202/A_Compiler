#include "backend/targets/utils/ra/method/costmodel/costmodel.h"
#include "context/backend_context.h"

namespace backend::targetir {
auto CostModel::CalculateCost([[maybe_unused]] TargetVirtualReg *vreg) -> double {
  // TODO(loki): Implement the cost calculation logic here, e.g., based on the live range length, usage frequency, etc.
  return 0;
}

auto CostModel::CalculatePriority([[maybe_unused]] TargetVirtualReg *vreg) -> double {
  // TODO(loki): Implement the priority calculation logic here, e.g., based on the cost, degree in the interference
  // graph, etc.
  return 0;
}

}  // namespace backend::targetir