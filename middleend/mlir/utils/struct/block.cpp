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

auto Block::PrintIndent(std::ofstream *out) const -> void {
  for (int i = 0; i < indent_level_; ++i) {
    *out << "  ";
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
auto Block::Emit(std::ofstream *out) -> void {
  PrintIndent(out);
  if (block_name_.empty()) {
    *out << "block (";
  } else {
    *out << "%" << block_name_ << block_id_ << " (";
  }
  for (auto argument : arguments_) {
    argument->Emit(out);
    if (argument != arguments_.back()) {
      *out << ", ";
    }
  }
  *out << ") {\n";
  indent_level_++;
  for (auto op : operations_) {
    op->SetIndentLevel(indent_level_);
    op->Emit(out);
  }
  indent_level_--;
  PrintIndent(out);
  *out << "}\n";
}

}  // namespace middleend::mlir