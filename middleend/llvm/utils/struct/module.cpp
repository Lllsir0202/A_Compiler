#include "middleend/llvm/utils/struct/module.h"
#include "context/compiler_context.h"
#include "middleend/llvm/inst/func/funcdef.h"

namespace middleend::llvm::utils {

Module::~Module() {
  for (auto func : functions_) {
    delete func;
  }
  for (auto &pair : func_def_insts_) {
    delete pair.second;
  }
  for (auto &pair : global_var_defs_) {
    delete pair.second;
  }
  for (auto func_def : library_func_def_insts_) {
    delete func_def;
  }
}

auto Module::Emit(std::ofstream *out) const -> void {
  // Emit the module in llvm ir format
  // Emit the library functions first
  for (const auto &func_def : library_func_def_insts_) {
    func_def->Emit(out);
  }
  // Emit the global variable definitions
  for (const auto &pair : global_var_defs_) {
    pair.second->Emit(out);
  }
  // Finally emit the user-defined function definitions
  for (const auto &func : functions_) {
    auto func_def = GetFunctionDef(func->GetFuncName());
    if (func_def == nullptr) {
      throw std::logic_error("Function definition not found for function: " + func->GetFuncName());
    }
    func_def->Emit(out);
    func->Emit(out);
  }
}
}  // namespace middleend::llvm::utils
