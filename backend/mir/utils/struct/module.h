#ifndef BACKEND_MIR_UTILS_STRUCT_MODULE_H
#define BACKEND_MIR_UTILS_STRUCT_MODULE_H
#include <string>
#include <unordered_map>
#include <vector>
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

 private:
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