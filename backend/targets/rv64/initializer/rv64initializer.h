#ifndef BACKEND_TARGETS_RV64_INITIALIZER_RV64INITIALIZER_H
#define BACKEND_TARGETS_RV64_INITIALIZER_RV64INITIALIZER_H

#include "backend/targets/utils/manager/targetinitializer.h"
namespace context {
class BackendContext;
}  // namespace context

namespace backend::targetir::rv64 {
class RV64TargetInitializer : public TargetInitializer {
 private:
  auto InitPhysicalRegPool() -> void override;
  context::BackendContext *context_;
};

}  // namespace backend::targetir::rv64

#endif