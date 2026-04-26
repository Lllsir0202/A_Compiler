#ifndef MIDDLEEND_MLIR_UTILS_STRUCT_REGION_H
#define MIDDLEEND_MLIR_UTILS_STRUCT_REGION_H
#include <fstream>
#include <vector>

namespace middleend::mlir {
class Block;
class Region {
 public:
  ~Region();
  /**
   * @brief print the region
   */
  auto Emit(std::ofstream *out) const -> void;
  auto AddBlock(Block *block) -> void { blocks_.emplace_back(block); }
  // The region is used to represent the control flow of the program.
  std::vector<Block *> blocks_;
};

}  // namespace middleend::mlir
#endif