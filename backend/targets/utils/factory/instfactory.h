#ifndef BACKEND_TARGETS_UTILS_FACTORY_INSTFACTORY_H
#define BACKEND_TARGETS_UTILS_FACTORY_INSTFACTORY_H

#include "backend/targets/utils/inst/targetinst.h"

namespace context {
class BackendContext;
}

namespace backend::targetir {
class InstFactory {
  template <typename T, typename... Args>
  static auto CreateInst(Args &&...args) -> T * {
    return T::Create(std::forward<Args>(args)...);
  }

  friend class context::BackendContext;
};

}  // namespace backend::targetir

#endif