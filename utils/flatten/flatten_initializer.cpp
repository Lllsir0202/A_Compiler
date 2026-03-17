#include "utils/flatten/flatten_initializer.h"
#include "frontend/ast/ast_decl.h"

namespace flatten {
auto FlattenInitializer([[maybe_unused]] const frontend::ast::InitValNode &init_node) -> initializer::Initializer {
  // TODO(loki): implement this function
  return {};
}

auto FlattenInitializer([[maybe_unused]] const frontend::ast::InitListNode &init_node) -> initializer::Initializer {
  // TODO(loki): implement this function
  return {};
}

}  // namespace flatten