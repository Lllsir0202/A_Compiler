#ifndef UTILS_FLATTEN_FLATTEN_INITIALIZER_H
#define UTILS_FLATTEN_FLATTEN_INITIALIZER_H
#include <vector>
#include "ast/ast_decl.h"
#include "utils/flatten/initializer.h"
namespace frontend::ast {
class InitValNode;
class InitListNode;
}  // namespace frontend::ast

namespace flatten {
/**
 * @brief Flatten the initializer list to a single value, used for constant expression evaluation
 * e.g., int a[2] = {1, 2}; -> int a[2] = 1, 2;
 * @param init_node: the initializer node to be flattened
 * @return the flattened initializer
 */
auto FlattenInitializer(const frontend::ast::InitListNode *init_node) -> initializer::InitializerPtr;

}  // namespace flatten

#endif