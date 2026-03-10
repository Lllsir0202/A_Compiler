#include "context/symboltable_context.h"
#include <cstddef>
#include <utility>

namespace context {
auto SymbolTableContext::ExistsInCurrentScope(const std::string &name) -> bool {
  if (var_table_.empty()) {
    return false;  // No scopes available
  }
  const auto &current_scope = var_table_.back();
  return current_scope.count(name) > 0;
}

auto SymbolTableContext::LookupVar(const std::string &name) -> symboltable::VarEntry * {
  for (auto it = var_table_.rbegin(); it != var_table_.rend(); ++it) {
    auto &scope = *it;
    if (scope.count(name) > 0) {
      return &scope[name];
    }
  }
  return nullptr;  // Not found
}

auto SymbolTableContext::LookupFunc(const std::string &name) -> symboltable::FuncEntry * {
  if (func_table_.count(name) > 0) {
    return &func_table_[name];
  }
  return nullptr;  // Not found
}

auto SymbolTableContext::AddVar(const std::string &name, type::Type *type, bool is_const,
                                std::vector<std::size_t> dimensions) -> bool {
  if (var_table_.empty()) {
    throw std::logic_error("No scope available to add variable");
  }
  if (ExistsInCurrentScope(name)) {
    return false;  // Variable already exists in the current scope
  }
  var_table_.back()[name] = {name, type, is_const, std::move(dimensions)};
  return true;
}

auto SymbolTableContext::AddFunc(const std::string &name, type::Type *return_type,
                                 std::vector<type::Type *> param_types) -> bool {
  if (func_table_.count(name) > 0) {
    return false;  // Function already exists
  }
  func_table_[name] = {name, return_type, std::move(param_types)};
  return true;
}

}  // namespace context