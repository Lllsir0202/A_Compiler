#include "backend/mir/utils/struct/module.h"
#include "backend/mir/utils/struct/function.h"

namespace backend::utils {
Module::~Module() {
  for (auto func : functions_) {
    delete func;
  }
}

}  // namespace backend::utils