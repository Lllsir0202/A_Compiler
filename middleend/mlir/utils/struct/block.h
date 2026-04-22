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
  auto Emit(std::ofstream *out) const -> void;

  // Interface to add an operation to the block
  // auto AddOperation(Operation *op) -> void { operations_.emplace_back(op); }

  // The vector of arguments is needed for i.e. while(i < 10 || j < 20)
  std::vector<Value *> arguments_;  // the arguments of this block
  // The vector of operations in the block
  std::vector<Operation *> operations_;  // the operations in this block
};

}  // namespace middleend::mlir

#endif