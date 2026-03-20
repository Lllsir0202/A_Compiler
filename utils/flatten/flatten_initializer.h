#ifndef UTILS_FLATTEN_FLATTEN_INITIALIZER_H
#define UTILS_FLATTEN_FLATTEN_INITIALIZER_H
#include "ast/ast_decl.h"
#include "utils/flatten/initializer.h"
namespace frontend::ast {
class Initializer;
class InitValNode;
class InitListNode;
}  // namespace frontend::ast

namespace flatten {
using LeafHandle = initializer::InitializerPtr(frontend::ast::Initializer *init_node);

/**
 * @brief Flatten the initializer list to a single value, used for constant expression evaluation
 * e.g., int a[2] = {1, 2}; -> int a[2] = 1, 2;
 * @param init_node: the initializer node to be flattened
 * @param cursor: the current position in the initializer list, used for recursive flattening
 * @param dims&: the dimensions of the array, used for calculating the total size of the array
 * @param total_size&: the total size of the array, used for padding
 * @param depth: the current depth of the initializer list
 * @param function_pointer: the function pointer used for figure out how to get the value.
 * @return the flattened initializer
 */
auto FlattenInitializer(frontend::ast::Initializer *init_node, std::unique_ptr<initializer::ArrayInitializer> &results,
                        size_t &cursor, const std::vector<int> &dims, const size_t &total_size, size_t depth,
                        LeafHandle function_pointer) -> void;

/**
 * @brief handle the initializer list is too deep
 * @use: just get the first scalar initializer in the deep loop
 */
auto HandleDeepInitializer(frontend::ast::Initializer *init_node,
                           LeafHandle function_pointer) -> initializer::InitializerPtr;

}  // namespace flatten

#endif