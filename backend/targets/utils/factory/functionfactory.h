#ifndef BACKEND_TARGETS_UTILS_FACTORY_FUNCTIONFACTORY_H
#define BACKEND_TARGETS_UTILS_FACTORY_FUNCTIONFACTORY_H

#include "backend/targets/utils/struct/targetframeobject.h"
#include "backend/targets/utils/struct/targetfunction.h"

namespace context {
class BackendContext;
}

namespace backend::targetir {
class FunctionFactory {
  static auto CreateTargetFunction(const std::string &name) -> TargetFunction * { return new TargetFunction(name); }

  // Since frame objects are stored in the function
  static auto CreateTargetFrameObject(size_t id, size_t size, size_t alignment) -> TargetFrameObject * {
    return new TargetFrameObject(id, size, alignment);
  }
  friend class context::BackendContext;
};

}  // namespace backend::targetir

#endif