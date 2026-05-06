#ifndef MIDDLEEND_LLVM_UTILS_FACTORY_INSTFACTORY_H
#define MIDDLEEND_LLVM_UTILS_FACTORY_INSTFACTORY_H

#include <utility>
namespace context {
class LLVMIRResourceContext;

}  // namespace context
namespace middleend::llvm::utils::factory {
class InstFactory {
 private:
  template <typename InstType, typename... Args>
  static auto CreateInst(Args &&...args) -> InstType * {
    auto *inst = new InstType(std::forward<Args>(args)...);
    return inst;
  }

  friend class context::LLVMIRResourceContext;
};

}  // namespace middleend::llvm::utils::factory

#endif