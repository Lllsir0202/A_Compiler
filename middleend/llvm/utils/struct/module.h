#ifndef MIDDLEEND_LLVM_UTILS_STRUCT_MODULE_H
#define MIDDLEEND_LLVM_UTILS_STRUCT_MODULE_H
#include <unordered_map>
#include "llvm/inst/memory/globaldef.h"
#include "middleend/llvm/utils/struct/function.h"
#include "mlir/dialect/var/globaldef.h"

namespace middleend::llvm::inst {
class FuncDefInst;
}  // namespace middleend::llvm::inst

namespace middleend::mlir::var {
class VarDefOp;
}

namespace middleend::llvm::utils {
/**
 * @brief Module class defines the llvm module,
 * which is the topest level of llvmir ,it contains a list of functions
 */
class Module {
 public:
  auto GetFunctions() const -> const std::vector<Function *> & { return functions_; }
  auto AddFunction(Function *function) -> void { functions_.emplace_back(function); }

  auto GetFunctionDef(const std::string &func_name) -> middleend::llvm::inst::FuncDefInst * {
    auto it = func_def_insts_.find(func_name);
    if (it != func_def_insts_.end()) {
      return it->second;
    }
    return nullptr;
  }

  auto AddFunctionDef(const std::string &func_name, middleend::llvm::inst::FuncDefInst *func_def) -> void {
    if (func_def_insts_.count(func_name) > 0) {
      throw std::logic_error("Function definition already exists: " + func_name);
    }
    func_def_insts_[func_name] = func_def;
  }

  // Global variables will be emit at the top of the llvm ir
  auto GetGlobalVarDef(const std::string &var_name) -> middleend::llvm::inst::GlobalDefInst * {
    auto it = global_var_defs_.find(var_name);
    if (it != global_var_defs_.end()) {
      return it->second;
    }
    return nullptr;
  }

  auto AddGlobalVarDef(const std::string &var_name, middleend::llvm::inst::GlobalDefInst *global_var_def) -> void {
    if (global_var_defs_.count(var_name) > 0) {
      throw std::logic_error("Global variable definition already exists: " + var_name);
    }
    global_var_defs_[var_name] = global_var_def;
  }

 private:
  // Record all functions
  std::vector<Function *> functions_;

  // Record the function definition instruction
  std::unordered_map<std::string, middleend::llvm::inst::FuncDefInst *> func_def_insts_;

  // Record the global variable definition instruction, which will be emitted at the top of the llvm ir
  std::unordered_map<std::string, middleend::llvm::inst::GlobalDefInst *> global_var_defs_;
};

}  // namespace middleend::llvm::utils

#endif