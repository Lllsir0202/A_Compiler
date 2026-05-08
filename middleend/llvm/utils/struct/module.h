#ifndef MIDDLEEND_LLVM_UTILS_STRUCT_MODULE_H
#define MIDDLEEND_LLVM_UTILS_STRUCT_MODULE_H
#include <unordered_map>
#include "middleend/llvm/utils/struct/function.h"

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
  auto GetGlobalVars() const -> const std::vector<mlir::var::VarDefOp *> & { return global_vars_; }
  auto AddGlobalVar(mlir::var::VarDefOp *global_var) -> void { global_vars_.emplace_back(global_var); }

 private:
  // Record all functions
  std::vector<Function *> functions_;

  // Record all global variables
  std::vector<mlir::var::VarDefOp *> global_vars_;

  // Record the function definition instruction
  std::unordered_map<std::string, middleend::llvm::inst::FuncDefInst *> func_def_insts_;
};

}  // namespace middleend::llvm::utils

#endif