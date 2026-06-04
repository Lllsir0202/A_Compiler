#include "backend/mir/utils/struct/module.h"
#include "backend/mir/inst/func/funcdeclare.h"
#include "backend/mir/inst/func/funcdef.h"
#include "backend/mir/inst/memory/globaldef.h"
#include "backend/mir/utils/struct/function.h"

namespace backend::utils {
Module::~Module() {
  for (auto func : functions_) {
    delete func;
  }
}

auto Module::Reset() -> void {
  for (auto func : functions_) {
    delete func;
  }
  functions_.clear();
  func_def_insts_.clear();
  func_declare_insts_.clear();
  global_var_insts_.clear();
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

auto Module::AddFuncDefInst(inst::FuncDefInst *func_def_inst) -> void {
  auto name = func_def_inst->GetFuncName();
  if (func_def_insts_.count(name) > 0) {
    return;
  }
  func_def_insts_[name] = func_def_inst;
}

auto Module::AddFuncDeclareInst(inst::FuncDeclareInst *func_declare_inst) -> void {
  auto name = func_declare_inst->GetFuncName();
  if (func_declare_insts_.count(name) > 0) {
    return;
  }
  func_declare_insts_[name] = func_declare_inst;
}

auto Module::AddGlobalDefInst(inst::GlobalDefInst *global_def_inst) -> void {
  auto name = global_def_inst->GetName();
  if (global_var_insts_.count(name) > 0) {
    return;
  }
  global_var_insts_[name] = global_def_inst;
}

}  // namespace backend::utils