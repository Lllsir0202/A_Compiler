#include "backend/targets/utils/struct/targetmodule.h"

namespace backend::targetir {
TargetModule::~TargetModule() {
  for (auto function : functions_) {
    delete function;
  }
  functions_.clear();
}

auto TargetModule::Emit(std::ofstream *out) -> void {
  for (auto function : functions_) {
    function->Emit(out);
  }
}

}  // namespace backend::targetir