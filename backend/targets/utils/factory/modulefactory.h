#ifndef BACKEND_TARGETS_UTILS_FACTORY_MODULEFACTORY_H
#define BACKEND_TARGETS_UTILS_FACTORY_MODULEFACTORY_H
#include "backend/targets/utils/struct/targetmodule.h"

namespace context {
class BackendContext;
}
namespace backend::targetir {
class ModuleFactory {
  static auto CreateTargetModule() -> TargetModule * { return new TargetModule(); }
  friend class context::BackendContext;
};

}  // namespace backend::targetir

#endif