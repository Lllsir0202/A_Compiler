#include "backend/targets/utils/struct/targetfunction.h"
#include "backend/targets/utils/struct/targetblock.h"

namespace backend::targetir {
// Blocks are owned by the TargetContext (single ownership), so the function does
// not delete them here — doing so would double-free.
TargetFunction::~TargetFunction() = default;

auto TargetFunction::Emit(std::ofstream *out) -> void {
  // Output this function's signature
  *out << name_ << ":\n";
  for (auto block : blocks_) {
    block->Emit(out);
  }
}

}  // namespace backend::targetir