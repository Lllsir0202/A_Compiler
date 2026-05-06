#ifndef MIDDLEEND_LLVM_UTILS_STRUCT_BLOCK_H
#define MIDDLEEND_LLVM_UTILS_STRUCT_BLOCK_H

#include <cstddef>
namespace middleend::llvm::utils::factory {
// Forward declare the block factory to allow it to create blocks
class BlockFactory;
}  // namespace middleend::llvm::utils::factory

namespace middleend::llvm::utils {
/**
 * @brief This class defines the basic block for llvmir,
 * and it should contain a list of instructions, and a unique block id,
 */
class BasicBlock {
 public:
 private:
  explicit BasicBlock(size_t id) : id_(id) {}
  size_t id_;
  // TODO(loki): should contain a container of instructions
  friend class factory::BlockFactory;
};

}  // namespace middleend::llvm::utils

#endif