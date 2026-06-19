#ifndef BACKEND_MIR_UTILS_STRUCT_FUNCTION_H
#define BACKEND_MIR_UTILS_STRUCT_FUNCTION_H
#include <unordered_map>
#include <vector>
#include "utils/type/type.h"

namespace backend::utils::factory {
class FunctionFactory;
}
namespace backend::utils {
class Block;
class FrameInfo;
// This class represents the function struct in the mir
class Function {
 public:
  ~Function();
  auto Emit(std::ofstream *out) const -> void;
  // Interface to access the function information
  auto GetName() const -> const std::string & { return func_name_; }
  auto GetReturnType() const -> type::Type * { return return_type_; }
  auto GetBlocks() const -> const std::vector<Block *> & { return blocks_; }
  auto AddBlock(Block *block) -> void { blocks_.emplace_back(block); }
  auto SetBlocks(std::vector<Block *> blocks) -> void { blocks_ = std::move(blocks); }
  // Record the map from id to pointer
  auto GetBlockById(size_t block_id) const -> Block * {
    auto it = block_by_id_.find(block_id);
    if (it != block_by_id_.end()) {
      return it->second;
    }
    return nullptr;
  }
  auto AddBlockById(size_t block_id, Block *block) -> void { block_by_id_[block_id] = block; }

  auto GetFrameInfoById(size_t frame_id) const -> FrameInfo *;
  auto AddFrameInfo(size_t size, size_t align) -> FrameInfo *;

 private:
  explicit Function(std::string func_name, type::Type *return_type)
      : func_name_(std::move(func_name)), return_type_(return_type) {}
  // A function records a list of blocks
  std::vector<Block *> blocks_;

  // Map from block id to block pointer
  std::unordered_map<size_t, Block *> block_by_id_;

  // Record the function name
  std::string func_name_;
  // Record the return type
  type::Type *return_type_;

  // A function has a stack frame
  std::vector<FrameInfo *> frame_infos_;

  friend class factory::FunctionFactory;
};

}  // namespace backend::utils

#endif