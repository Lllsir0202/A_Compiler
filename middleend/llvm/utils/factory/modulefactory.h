#ifndef MIDDLEEND_LLVM_UTILS_FACTORY_MODULEFACTORY_H
#define MIDDLEEND_LLVM_UTILS_FACTORY_MODULEFACTORY_H
#include "middleend/llvm/utils/struct/module.h"
namespace middleend::llvm::utils::factory {
class ModuleFactory {
 public:
  static auto CreateModule() -> Module * { return new Module(); }
};

}  // namespace middleend::llvm::utils::factory

#endif