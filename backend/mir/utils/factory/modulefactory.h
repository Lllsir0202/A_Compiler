#ifndef BACKEND_MIR_UTILS_FACTORY_MODULEFACTORY_H
#define BACKEND_MIR_UTILS_FACTORY_MODULEFACTORY_H

#include "backend/mir/utils/struct/module.h"

namespace backend::utils::factory {
class ModuleFactory {
 public:
  static auto CreateModule() -> Module * { return new Module(); }
};

}  // namespace backend::utils::factory

#endif  // BACKEND_MIR_UTILS_FACTORY_MODULEFACTORY_H