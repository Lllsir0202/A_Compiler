#include "backend/targets/utils/struct/targetfunction.h"
#include "backend/targets/utils/struct/targetblock.h"

namespace backend::targetir {
// Delete all blocks in the function
TargetFunction::~TargetFunction() {
  for (auto block : blocks_) {
    delete block;
  }
}

auto TargetFunction::Emit(std::ofstream *out) -> void {
  // Output this function's signature
  *out << name_ << ":\n";
  for (auto block : blocks_) {
    block->Emit(out);
  }
}

}  // namespace backend::targetir