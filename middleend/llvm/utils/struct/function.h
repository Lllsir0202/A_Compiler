/**
 * @brief This file defines the Function class,
 * which represents a function in LLVM IR.
 * @note We need to consider how to represent library functions
 */
#ifndef MIDDLEEND_LLVM_UTILS_STRUCT_FUNCTION_H
#define MIDDLEEND_LLVM_UTILS_STRUCT_FUNCTION_H
#include <string>
#include <vector>
#include "middleend/llvm/utils/struct/block.h"
#include "utils/type/type.h"
namespace middleend::llvm::utils::factory {
class FuncFactory;
}  // namespace middleend::llvm::utils::factory

namespace middleend::llvm::utils {
class Function {
 public:
  // Interfaces to get function information
  auto GetFuncName() const -> const std::string & { return func_name_; }
  auto GetReturnType() const -> type::Type * { return return_type_; }
  auto GetBasicBlocks() const -> const std::vector<BasicBlock *> & { return basic_blocks_; }
  auto GetEntryBlock() const -> BasicBlock * { return entry_block_; }

  // Add block
  auto AddBasicBlock(BasicBlock *block) -> void {
    basic_blocks_.emplace_back(block);
    // If the block is the first block, set it as the entry block
    if (basic_blocks_.size() == 1) {
      entry_block_ = block;
    }
  }

 private:
  // Only allow friend class to create
  explicit Function(std::string func_name, type::Type *return_type)
      : func_name_(std::move(func_name)), return_type_(return_type) {}
  friend class factory::FuncFactory;

  std::string func_name_;
  // Return type of the function
  type::Type *return_type_;

  // Containers, which should be a list of basic blocks
  std::vector<BasicBlock *> basic_blocks_;

  // Entry block, should be the index 0 of basic_blocks_
  // it should never be nullptr except for library functions
  BasicBlock *entry_block_;
};

}  // namespace middleend::llvm::utils

#endif