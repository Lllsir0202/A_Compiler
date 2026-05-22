#ifndef BACKEND_MIR_UTILS_STRUCT_MODULE_H
#define BACKEND_MIR_UTILS_STRUCT_MODULE_H
#include <vector>
namespace backend::utils {
class Function;
class Module {
 public:
  ~Module();

 private:
  /**
   * @brief A module represents a programme, and it should alse record:
   * 1. a list of functions
   * 2. a list of global variables
   * 3. a list of function declarations
   */
  std::vector<Function *> functions_;
  // TODO(loki): Add instructions' pointer after implementing the instructions' concrete class
};

}  // namespace backend::utils

#endif