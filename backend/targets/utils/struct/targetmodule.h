#ifndef BACKEND_TARGETS_UTILS_STRUCT_TARGETMODULE_H
#define BACKEND_TARGETS_UTILS_STRUCT_TARGETMODULE_H
#include "backend/targets/utils/struct/targetfunction.h"
namespace backend::targetir {
class TargetModule {
 public:
  // Destructor: delete all functions
  ~TargetModule();
  auto Emit(std::ofstream *out) -> void;
  auto AddFunction(TargetFunction *function) -> void { functions_.emplace_back(function); }

 private:
  // Constructor: create an empty module
  TargetModule() = default;
  friend class ModuleFactory;
  // Record a list of functions
  std::vector<TargetFunction *> functions_;
};

}  // namespace backend::targetir

#endif