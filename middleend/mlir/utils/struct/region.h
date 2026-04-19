#ifndef MIDDLEEND_MLIR_UTILS_STRUCT_REGION_H
#define MIDDLEEND_MLIR_UTILS_STRUCT_REGION_H
#include <vector>

namespace middleend::mlir {
class Block;
class Region {
 public:
  // The region is used to represent the control flow of the program.
  std::vector<Block *> blocks_;
};

}  // namespace middleend::mlir
#endif