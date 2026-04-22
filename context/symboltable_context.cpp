#include "context/symboltable_context.h"
#include <utility>

namespace context {
auto SymbolTableContext::ExistsInCurrentScope(const std::string &name) -> bool {
  /**
   * @cond: function parameter will be seen as in the first scope
   * since the first scope is for global variables,
   * and the second scope is for function parameters
   */
  if (current_func_ != nullptr && func_param_table_[current_func_].count(name) > 0 && var_table_.size() == 2) {
    return true;  // Variable exists in the current function parameters
  }

  if (var_table_.empty()) {
    return false;  // No scopes available
  }
  const auto &current_scope = var_table_.back();
  return current_scope.count(name) > 0;
}

auto SymbolTableContext::LookupVar(const std::string &name) -> symboltable::VarEntry * {
  symboltable::VarEntry *return_entry = nullptr;

  // Look up from the innermost scope to the outermost scope
  for (auto it = var_table_.rbegin(); it != var_table_.rend(); ++it) {
    auto &scope = *it;
    if (scope.count(name) > 0) {
      return_entry = scope[name];
      break;  // Found the variable, stop searching
    }
  }

  // if not found or found in global scope, look up if it exists in function parameters
  bool not_found_or_in_global_scope = return_entry == nullptr || return_entry->is_global_;

  // Look up if it exists in formal paramters
  if (not_found_or_in_global_scope && current_func_ != nullptr && func_param_table_[current_func_].count(name) > 0) {
    // Return a pointer to the function parameter entry
    return_entry = &func_param_table_[current_func_][name];
  }
  // return not found or the variable entry found in the scopes or function parameters
  return return_entry;  // Not found
}

auto SymbolTableContext::LookupFunc(const std::string &name) -> symboltable::FuncEntry * {
  if (func_table_.count(name) > 0) {
    return &func_table_[name];
  }
  return nullptr;  // Not found
}

auto SymbolTableContext::AddVar(const std::string &name, type::Type *type, bool is_const,
                                std::vector<int> dimensions) -> bool {
  // Check if the variable already exists in the current function parameters
  /**
   * @note: function parameter will be seen as in the first scope,
   * and since then, the same name variable cannot be defined in the function body
   * but it can be defined in the inner scope of the function body
   * which means the same name variable can be defined while the var_table.size() > 1
   */
  if (current_func_ != nullptr && func_param_table_[current_func_].count(name) > 0 && var_table_.size() == 2) {
    return false;  // Variable already exists in the current function parameters
  }

  if (var_table_.empty()) {
    throw std::logic_error("No scope available to add variable");
  }
  if (ExistsInCurrentScope(name)) {
    return false;  // Variable already exists in the current scope
  }
  auto entry = new symboltable::VarEntry{name, type, is_const, std::move(dimensions)};
  entry->symbol_id_ = entries_.size();  // Generate symbol id based on the size of entries
  entries_.emplace_back(entry);
  var_table_.back()[name] = entry;
  if (var_table_.size() == 1) {
    var_table_.back()[name]->is_global_ = true;  // Mark as global variable if it's in the global scope
  }
  return true;
}

auto SymbolTableContext::SetType(const std::string &name, type::Type *type) -> bool {
  if (!ExistsInCurrentScope(name)) {
    return false;  // Variable does not exist in the current scope
  }
  auto var_entry = LookupVar(name);
  var_entry->type_ = type;
  return true;
}

auto SymbolTableContext::SetConst(const std::string &name, bool is_const) -> bool {
  if (!ExistsInCurrentScope(name)) {
    return false;  // Variable does not exist in the current scope
  }
  auto var_entry = LookupVar(name);
  var_entry->is_const_ = is_const;
  return true;
}

auto SymbolTableContext::SetDimensions(const std::string &name, std::vector<int> dimensions) -> bool {
  if (!ExistsInCurrentScope(name)) {
    return false;  // Variable does not exist in the current scope
  }
  auto var_entry = LookupVar(name);
  var_entry->dimensions_ = std::move(dimensions);
  return true;
}

auto SymbolTableContext::AddFunc(const std::string &name, type::Type *return_type,
                                 std::vector<type::Type *> param_types) -> bool {
  if (func_table_.count(name) > 0) {
    return false;  // Function already exists
  }
  func_table_[name] = {name, return_type, std::move(param_types)};
  current_func_ = &func_table_[name];  // Set the current function being processed
  return true;
}

auto SymbolTableContext::AddFuncParam(symboltable::FuncEntry *func_entry, const std::string &name, type::Type *type,
                                      std::vector<int> dimensions) -> bool {
  if (func_entry == nullptr) {
    return false;
  }
  if (func_param_table_[func_entry].count(name) > 0) {
    return false;  // Function parameter already exists
  }
  func_param_table_[func_entry][name] = {name, type, false, std::move(dimensions), true};
  return true;
}

}  // namespace context