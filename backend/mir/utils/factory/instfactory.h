#ifndef BACKEND_MIR_UTILS_FACTORY_INSTFACTORY_H
#define BACKEND_MIR_UTILS_FACTORY_INSTFACTORY_H

#include <utility>
namespace context {
class MIRContext;
}
namespace backend::utils::factory {
class InstFactory {
  template <typename InstType, typename... Args>
  static auto CreateInst(Args &&...args) -> InstType * {
    auto *inst = InstType::Create(std::forward<Args>(args)...);
    return inst;
  }

  friend class context::MIRContext;
};
}  // namespace backend::utils::factory

#endif