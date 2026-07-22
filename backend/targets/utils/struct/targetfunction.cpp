#include "backend/targets/utils/struct/targetfunction.h"
#include "backend/targets/utils/struct/targetblock.h"
#include "backend/targets/utils/struct/targetframeobject.h"

namespace backend::targetir {
// Delete all blocks in the function
TargetFunction::~TargetFunction() {
  for (auto block : blocks_) {
    delete block;
  }
  blocks_.clear();
  for (auto frame_object : frame_objects_) {
    delete frame_object;
  }
  frame_objects_.clear();
}

auto TargetFunction::Emit(std::ofstream *out) -> void {
  // Output the literal pool for this function, if any
  if (!float_literal_pool_.empty()) {
    // Output the literal pool section
    *out << "    .section    .sdata,\"aw\",@progbits\n";
    *out << "    .p2align    2, 0x0\n";

    for (const auto &[_, entry] : float_literal_pool_) {
      *out << ".LCPI_" << entry.GetIndex() << ":\n";
      entry.Emit(out);
    }
  }

  // Output the function-related information
  *out << "    .text\n";
  *out << "    .globl " << name_ << "\n";
  *out << "    .p2align    1\n";
  *out << "    .type    " << name_ << ", @function\n";

  // Output this function's signature
  *out << name_ << ":\n";
  for (auto block : blocks_) {
    block->Emit(out);
  }
}

}  // namespace backend::targetir