#ifndef BACKEND_TARGETS_UTILS_FACTORY_MODULEFACTORY_H
#define BACKEND_TARGETS_UTILS_FACTORY_MODULEFACTORY_H
#include "backend/targets/utils/struct/targetmodule.h"
namespace backend::targetir {
class ModuleFactory {
  static auto CreateTargetModule() -> TargetModule * { return new TargetModule(); }
};

}  // namespace backend::targetir

#endif