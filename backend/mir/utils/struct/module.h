#ifndef BACKEND_MIR_UTILS_STRUCT_MODULE_H
#define BACKEND_MIR_UTILS_STRUCT_MODULE_H
#include <string>
#include <unordered_map>
#include <vector>
namespace backend::utils::factory {
class ModuleFactory;
}
namespace backend::inst {
class FuncDeclareInst;
class GlobalDefInst;
class FuncDefInst;
}  // namespace backend::inst
namespace backend::utils {
class Function;
class Module {
 public:
  ~Module();

  auto GetFuncDefByName(const std::string &name) -> inst::FuncDefInst *;
  auto GetFuncDeclareByName(const std::string &name) -> inst::FuncDeclareInst *;
  auto GetGlobalDefByName(const std::string &name) -> inst::GlobalDefInst *;

  auto AddFunction(Function *func) -> void { functions_.emplace_back(func); }
  auto AddFuncDefInst(inst::FuncDefInst *func_def_inst) -> void;
  auto AddFuncDeclareInst(inst::FuncDeclareInst *func_declare_inst) -> void;
  auto AddGlobalDefInst(inst::GlobalDefInst *global_def_inst) -> void;

  // Used in replace the module
  auto Reset() -> void {
    functions_.clear();
    func_def_insts_.clear();
    func_declare_insts_.clear();
    global_var_insts_.clear();
  }

 private:
  Module() = default;
  friend class factory::ModuleFactory;
  /**
   * @brief A module represents a programme, and it should alse record:
   * 1. a list of functions
   * 2. a list of global variables
   * 3. a list of function declarations
   */
  std::vector<Function *> functions_;

  // Map from name to instruction
  std::unordered_map<std::string, inst::FuncDefInst *> func_def_insts_;

  std::unordered_map<std::string, inst::FuncDeclareInst *> func_declare_insts_;

  std::unordered_map<std::string, inst::GlobalDefInst *> global_var_insts_;
};

}  // namespace backend::utils

#endif