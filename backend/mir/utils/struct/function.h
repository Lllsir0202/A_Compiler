#ifndef BACKEND_MIR_UTILS_STRUCT_FUNCTION_H
#define BACKEND_MIR_UTILS_STRUCT_FUNCTION_H
#include <vector>
#include "utils/type/type.h"

namespace backend::utils {
class Block;
// This class represents the function struct in the mir
class Function {
 public:
  ~Function();
  // Interface to access the function information
  auto GetName() const -> const std::string & { return func_name_; }
  auto GetReturnType() const -> type::Type * { return return_type_; }
  auto GetBlocks() const -> const std::vector<Block *> & { return blocks_; }
  auto AddBlock(Block *block) -> void { blocks_.emplace_back(block); }
  auto SetBlocks(std::vector<Block *> blocks) -> void { blocks_ = std::move(blocks); }

 private:
  explicit Function(std::string func_name, type::Type *return_type)
      : func_name_(std::move(func_name)), return_type_(return_type) {}
  // A function records a list of blocks
  std::vector<Block *> blocks_;
  // Record the function name
  std::string func_name_;
  // Record the return type
  type::Type *return_type_;
};

}  // namespace backend::utils

#endif