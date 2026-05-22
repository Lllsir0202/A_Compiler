#ifndef BACKEND_MIR_UTILS_FACTORY_INSTFACTORY_H
#define BACKEND_MIR_UTILS_FACTORY_INSTFACTORY_H

#include <utility>
namespace backend::utils {
class InstFactory {
  template <typename InstType, typename... Args>
  static auto CreateInst(Args &&...args) -> InstType * {
    auto *inst = InstType::Create(std::forward<Args>(args)...);
    return inst;
  }
};
}  // namespace  backend::utils

#endif