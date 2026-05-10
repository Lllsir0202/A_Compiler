#include "middleend/llvm/utils/struct/function.h"

namespace middleend::llvm::utils {
Function::~Function() {
  for (auto block : basic_blocks_) {
    delete block;
  }
  entry_block_ = nullptr;
}

// Has print function definition in advance
/** Format be like:
 * define i32 @main() {
 *   entry:
 *     %0 = add i32 1, 2
 *     ret i32 %0
 * }
 */

auto Function::Emit(std::ofstream *out) const -> void {
  *out << "{\n";
  for (const auto &block : basic_blocks_) {
    block->Emit(out);
  }
  *out << "}\n";
}

}  // namespace middleend::llvm::utils