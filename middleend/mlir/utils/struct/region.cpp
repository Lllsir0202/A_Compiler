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

/**
 * @brief emit the region
 * it will emit the blocks in the region
 */
auto Region::Emit(std::ofstream *out) const -> void {
  *out << "region {" << std::endl;
  for (auto block : blocks_) {
    block->Emit(out);
  }
  *out << "}" << std::endl;
}

}  // namespace middleend::mlir