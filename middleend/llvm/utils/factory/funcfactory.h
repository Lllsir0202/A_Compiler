#ifndef MIDDLEEND_LLVM_UTILS_FACTORY_FUNCFACTORY_H
#define MIDDLEEND_LLVM_UTILS_FACTORY_FUNCFACTORY_H
#include "middleend/llvm/utils/struct/function.h"
#include "utils/type/type.h"

// Forward declare
namespace context {
class LLVMIRResourceContext;
}  // namespace context

namespace middleend::llvm::utils {
class FuncFactory {
 private:
  /**
   * @brief Create a new function with the given parameters
   * @param func_name The name of the function
   * @param return_type The return type of the function
   */
  static auto CreateFunction(const std::string &func_name, type::Type *return_type) -> Function * {
    return new Function(func_name, return_type);
  }
  friend class context::LLVMIRResourceContext;
};

}  // namespace middleend::llvm::utils

#endif