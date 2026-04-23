#include "middleend/mlir/utils/struct/block.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/value/value.h"

namespace middleend::mlir {
/**
 * @brief the destructor of the block
 * since a block will contain multiple operations,
 * and these operations will not be reused by other blocks,
 * so we will delete these operations
 */
Block::~Block() {
  for (auto op : operations_) {
    delete op;
  }
}

/**
 * @brief emit the block
 * it will emit the operations in the block
 * i.e.
 * block (arguments) {
 *    op1
 *    op2
 * }
 */
auto Block::Emit(std::ofstream *out) const -> void {
  *out << "block (";
  for (auto argument : arguments_) {
    argument->Emit(out);
    if (argument != arguments_.back()) {
      *out << ", ";
    }
  }
  *out << ") {\n";
  for (auto op : operations_) {
    op->Emit(out);
  }
  *out << "}\n";
}

}  // namespace middleend::mlir