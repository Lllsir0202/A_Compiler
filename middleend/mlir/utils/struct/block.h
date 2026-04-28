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

  // The vector of arguments is needed for i.e. while(i < 10 || j < 20)
  std::vector<Value *> arguments_;  // the arguments of this block
  // The vector of operations in the block
  std::vector<Operation *> operations_;  // the operations in this block
  int indent_level_ = 0;                 // the indent level of this block, which is used for printing
};

}  // namespace middleend::mlir

#endif