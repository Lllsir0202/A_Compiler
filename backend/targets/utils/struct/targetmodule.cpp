#include "backend/targets/utils/struct/targetmodule.h"

#include <cstring>

namespace backend::targetir {

TargetModule::~TargetModule() {
  for (auto function : functions_) {
    delete function;
  }
  functions_.clear();
  for (auto global : global_objects_) {
    delete global;
  }
  global_objects_.clear();
  library_functions_.clear();
}

auto TargetModule::Emit(std::ofstream *out) -> void {
  for (auto function : functions_) {
    function->Emit(out);
  }

  *out << "\n";

  for (auto global : global_objects_) {
    global->Emit(out);
    *out << "\n";
  }

  // In the end, emit the global variables and library functions
  if (!library_functions_.empty()) {
    *out << "    .addrsig\n";
  }
  for (auto &lib_func : library_functions_) {
    *out << "    .addrsig_sym " << lib_func;
    if (lib_func != library_functions_.back()) {
      *out << "\n";
    }
  }
}

}  // namespace backend::targetir
