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

auto Module::Emit(std::ofstream *out) const -> void {
  // Emit the module in llvm ir format
  // Emit the library functions first
  for (const auto &func_declare : func_declare_insts_) {
    func_declare.second->Emit(out);
  }
  // Emit the global variable definitions
  for (const auto &pair : global_var_insts_) {
    pair.second->Emit(out);
  }
  // Finally emit the user-defined function definitions
  for (const auto &func : functions_) {
    auto func_def = GetFuncDefByName(func->GetName());
    if (func_def == nullptr) {
      throw std::logic_error("Function definition not found for function: " + func->GetName());
    }
    func_def->Emit(out);
    func->Emit(out);
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

auto Module::GetFuncDefByName(const std::string &name) const -> inst::FuncDefInst * {
  auto it = func_def_insts_.find(name);
  if (it != func_def_insts_.end()) {
    return it->second;
  }
  return nullptr;
}

auto Module::GetFuncDeclareByName(const std::string &name) const -> inst::FuncDeclareInst * {
  auto it = func_declare_insts_.find(name);
  if (it != func_declare_insts_.end()) {
    return it->second;
  }
  return nullptr;
}

auto Module::GetGlobalDefByName(const std::string &name) const -> inst::GlobalDefInst * {
  auto it = global_var_insts_.find(name);
  if (it != global_var_insts_.end()) {
    return it->second;
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