#include "backend/mir/utils/struct/function.h"
#include "backend/mir/utils/struct/block.h"
namespace backend::utils {
Function::~Function() {
  for (auto *block : blocks_) {
    delete block;
  }
  blocks_.clear();
}

}  // namespace backend::utils