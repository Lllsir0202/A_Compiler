#include "backend/targets/utils/struct/targetmodule.h"

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
}

}  // namespace backend::targetir
