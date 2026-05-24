#include "backend/mir/utils/struct/module.h"
#include "backend/mir/utils/struct/function.h"

namespace backend::utils {
Module::~Module() {
  for (auto func : functions_) {
    delete func;
  }
}

auto Module::GetFuncDefByName(const std::string &name) -> inst::FuncDefInst * {
  if (func_def_insts_.count(name) > 0) {
    return func_def_insts_[name];
  }
  return nullptr;
}

auto Module::GetFuncDeclareByName(const std::string &name) -> inst::FuncDeclareInst * {
  if (func_declare_insts_.count(name) > 0) {
    return func_declare_insts_[name];
  }
  return nullptr;
}

auto Module::GetGlobalDefByName(const std::string &name) -> inst::GlobalDefInst * {
  if (global_var_insts_.count(name) > 0) {
    return global_var_insts_[name];
  }
  return nullptr;
}

}  // namespace backend::utils