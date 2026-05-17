#ifndef MIDDLEND_MLIR_UTILS_STRUCT_BLOCK_H
#define MIDDLEND_MLIR_UTILS_STRUCT_BLOCK_H
#include <fstream>
#include <vector>

namespace middleend::mlir {
class Operation;
class Value;
class Block {
 public:
  ~Block();
  // Emit the block
  auto Emit(std::ofstream *out) -> void;
  auto PrintIndent(std::ofstream *out) const -> void;

  // Interface to add an operation to the block
  auto AddOperation(Operation *op) -> void { operations_.emplace_back(op); }
  auto AddArgument(Value *arg) -> void { arguments_.emplace_back(arg); }
  auto RemoveOperation(Operation *op) -> void {
    std::vector<Operation *> new_ops;
    for (auto &operation : operations_) {
      if (operation != op) {
        new_ops.emplace_back(operation);
      }
    }
    operations_ = std::move(new_ops);
  }

  // The vector of arguments is needed for i.e. while(i < 10 || j < 20)
  std::vector<Value *> arguments_;  // the arguments of this block
  // The vector of operations in the block
  std::vector<Operation *> operations_;  // the operations in this block
  int indent_level_ = 0;                 // the indent level of this block, which is used for printing

  /**
   * @brief Used in `base` dialect, decide which block to jump to
   */
  int block_id_;            // the unique id of this block, which is used for printing and verification
  std::string block_name_;  // the name of this block, which is used for printing and verification
  auto Name() const -> std::string { return "%" + block_name_ + std::to_string(block_id_); }
};

}  // namespace middleend::mlir

#endif