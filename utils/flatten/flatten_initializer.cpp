#include "utils/flatten/flatten_initializer.h"
#include <cstddef>
#include <stdexcept>
#include "ast/value/value.h"
#include "frontend/ast/ast_decl.h"
#include "utils/flatten/initializer.h"

namespace flatten {
/**
 * @brief transfer a single initializer node to our specific data type
 * @note this function should only be called when:
 * 1. the initializers are all constant expressions
 * while in the semantic phase
 * 2. initializing a variable in IR stage,
 * since we need to represent the initializer to initialize them physically
 * so we will not do the check in this function,
 * including but not limited to: whether a pointer is a null pointer
 */
auto FlattenInitializer([[maybe_unused]] const frontend::ast::InitListNode *init_node) -> initializer::InitializerPtr {
  return nullptr;
}
}  // namespace flatten