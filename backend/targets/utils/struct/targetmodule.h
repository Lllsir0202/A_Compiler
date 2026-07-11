#ifndef BACKEND_TARGETS_UTILS_STRUCT_TARGETMODULE_H
#define BACKEND_TARGETS_UTILS_STRUCT_TARGETMODULE_H

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "backend/targets/utils/struct/targetfunction.h"
#include "backend/targets/utils/struct/targetglobalobject.h"
namespace backend::targetir {
class TargetFloatLiteralPoolEntry {
 public:
  TargetFloatLiteralPoolEntry(std::string label, std::uint32_t bits) : label_(std::move(label)), bits_(bits) {}

  auto GetLabel() const -> const std::string & { return label_; }
  auto GetBits() const -> std::uint32_t { return bits_; }

 private:
  std::string label_;
  std::uint32_t bits_;
};

class TargetModule {
 public:
  // Destructor: delete all functions
  ~TargetModule();
  auto Emit(std::ofstream *out) -> void;
  auto AddFunction(TargetFunction *function) -> void { functions_.emplace_back(function); }
  auto GetFunctions() -> const std::vector<TargetFunction *> & { return functions_; }
  auto AddGlobalObject(TargetGlobalObject *global_object) -> void { global_objects_.emplace_back(global_object); }
  auto GetGlobalObjects() const -> const std::vector<TargetGlobalObject *> & { return global_objects_; }
  auto AddLibraryFunction(const std::string &lib_func_name) -> void { library_functions_.emplace_back(lib_func_name); }
  auto GetLibraryFunctions() const -> const std::vector<std::string> & { return library_functions_; }

  auto GetOrCreateFloatLiteralLabel(float value) -> std::string;
  auto GetFloatLiteralPool() const -> const std::vector<TargetFloatLiteralPoolEntry> & { return float_literal_pool_; }

  auto Reset() -> void {
    for (auto *func : functions_) {
      delete func;
    }
    functions_.clear();
    for (auto *global : global_objects_) {
      delete global;
    }
    global_objects_.clear();
    library_functions_.clear();
    float_literal_pool_.clear();
    float_literal_pool_index_.clear();
  }

 private:
  // Constructor: create an empty module
  TargetModule() = default;
  friend class ModuleFactory;
  // Record a list of functions
  std::vector<TargetFunction *> functions_;

  // Record a list of global variables
  std::vector<TargetGlobalObject *> global_objects_;

  // Record a list of library functions
  // Need to record the name of the library function
  std::vector<std::string> library_functions_;

  // Literal pool, which will be used to store the literal values in the module
  std::vector<TargetFloatLiteralPoolEntry> float_literal_pool_;
  std::unordered_map<std::uint32_t, size_t> float_literal_pool_index_;
};

}  // namespace backend::targetir

#endif
