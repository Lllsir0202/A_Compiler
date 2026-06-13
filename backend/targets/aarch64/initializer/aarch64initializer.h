#ifndef BACKEND_TARGETS_AARCH64_INITIALIZER_AARCH64INITIALIZER_H
#define BACKEND_TARGETS_AARCH64_INITIALIZER_AARCH64INITIALIZER_H

#include "backend/targets/utils/manager/targetinitializer.h"
namespace context {
class BackendContext;
}  // namespace context

namespace backend::targetir::aarch64 {
class AArch64TargetInitializer : public TargetInitializer {
 private:
  auto InitPhysicalRegPool(context::BackendContext *context) -> void override;
  auto InitTargetPrinter() -> void override;
  friend class context::BackendContext;
};

}  // namespace backend::targetir::aarch64

#endif