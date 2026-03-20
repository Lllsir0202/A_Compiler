#include "utils/flatten/flatten_initializer.h"
#include <cstddef>
#include <memory>
#include "frontend/ast/ast_decl.h"
#include "utils/flatten/initializer.h"

namespace flatten {
/**
 * @brief get the first scalar initializer in the deep loop
 * @use: handle the initializer list is too deep, just get the first scalar initializer
 * @example:
 * int a[10] = { {{1}}, 2 };
 * when we meet { {{1}}, 2} , it is a init_list, the depth is 0;
 * then enter the first block, it is {{1}}, now depth is 1,
 * but the array only has one dimension, so the {{1}} should legally be 1
 * so we will just get the first scalar initializer in {{1}},
 * which means when the depth >= dims.size(), if the initializer is a list, we will just get the first scalar
 * however, if the initializer is not a list,
 * it has no difference that whether we get the first scalar initializer or not.
 */
auto HandleDeepInitializer(frontend::ast::Initializer *init_node,
                           LeafHandle function_pointer) -> initializer::InitializerPtr {
  while (init_node != nullptr && !init_node->IsScalar()) {
    // since the init_node is NOT scalar, it must be an init list
    auto init_list = static_cast<frontend::ast::InitListNode *>(init_node);
    if (!init_list->init_values_.empty()) {
      init_node = init_list->init_values_[0].get();
      continue;
    }
    init_node = nullptr;
  }
  // means we have reached the end of the deep loop
  // no matter it is a scalar initializer or a null sbracet
  return function_pointer(init_node);
}

/**
 * @brief transfer a single initializer node to our specific data type
 * @note this function should only be called when:
 * 1. the initializers are all constant expressions
 * while in the semantic phase
 * 2. initializing a variable in IR stage,
 * since we need to represent the initializer to initialize them physically
 * so we will not do the check in this function,
 * including but not limited to: whether a pointer is a null pointer
 * @note this function will only be called for initlist values
 * since for initval, we can directly evaluate the expression and get the value without flattening
 */
auto FlattenInitializer([[maybe_unused]] frontend::ast::Initializer *init_node,
                        std::unique_ptr<initializer::ArrayInitializer> &results, size_t &cursor,
                        const std::vector<int> &dims, const size_t &total_size, size_t depth,
                        LeafHandle function_pointer) -> void {
  if (cursor >= total_size) {
    throw std::runtime_error("Initializer list is too long, exceeds the total size of the array");
    return;
  }

  if (depth >= dims.size()) {
    // means the initializer list is too deep
    // we will just get the only initializer in the deep loop
    // use a function to get just the first scalar initializer
    auto init = HandleDeepInitializer(init_node, function_pointer);
    results->AddInitializer(std::move(init));
    cursor++;
    return;
  }

  // then deal with the normal case
  if (init_node->IsScalar()) {
    // means we have reached the end of the initializer list, we can get the value and return
    auto init = function_pointer(init_node);
    results->AddInitializer(std::move(init));
    cursor++;
    return;
  }

  // means the init_node is an init list
  auto init_list = static_cast<frontend::ast::InitListNode *>(init_node);
  for (auto &init : init_list->init_values_) {
    // note that we have entered a new block, so the depth should be added by 1
    FlattenInitializer(init.get(), results, cursor, dims, total_size, depth + 1, function_pointer);
  }

  // means the initializer list has finished, we should fill up the default value
  // padding
  int sub_block = 1;
  for (size_t i = depth + 1; i < dims.size(); i++) {
    sub_block *= dims[i];
  }
  int filled = cursor % sub_block;
  while (filled < sub_block && cursor < total_size) {
    results->AddInitializer(nullptr);
    cursor++;
    filled++;
  }
}
}  // namespace flatten