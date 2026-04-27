#include "middleend/mlir/utils/struct/region.h"
#include "middleend/mlir/utils/struct/block.h"

namespace middleend::mlir {
/**
 * @brief the destructor of the region
 * since a region will contain multiple blocks,
 * and these blocks will not be reused by other regions,
 * so we will delete these blocks
 */
Region::~Region() {
  for (auto block : blocks_) {
    delete block;
  }
}

auto Region::PrintIndent(std::ofstream *out) const -> void {
  for (int i = 0; i < indent_level_; ++i) {
    *out << "  ";
  }
}

/**
 * @brief emit the region
 * it will emit the blocks in the region
 */
auto Region::Emit(std::ofstream *out) -> void {
  PrintIndent(out);
  *out << "region {\n";
  indent_level_++;
  for (auto block : blocks_) {
    block->indent_level_ = indent_level_;
    block->Emit(out);
  }
  indent_level_--;
  PrintIndent(out);
  *out << "}\n";
}

}  // namespace middleend::mlir