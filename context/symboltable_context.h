#ifndef CONTEXT_SYMBOLTABLE_CONTEXT_H
#define CONTEXT_SYMBOLTABLE_CONTEXT_H
#include <cstddef>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include "utils/type/type.h"

namespace symboltable {
class SymbolTableFactory;
/**
 * @brief VarEntry representing a variable entry in the symbol table
 */
struct VarEntry {
  std::string name_;
  type::Type *type_;
  bool is_const_;
  std::vector<size_t> dimensions_;  // for array variables, store the dimensions of the array
};

/**
 * @brief FuncEntry representing a function entry in the symbol table
 */
struct FuncEntry {
  std::string name_;
  type::Type *return_type_;
  std::vector<type::Type *> param_types_;
};
}  // namespace symboltable

namespace context {

/**
 * @method: EnterScope
 * @method: ExitScope
 * @method: ExistsInCurrentScope
 * @method: LookupVar
 * @method: LookupFunc
 * @method: AddVar
 * @method: AddFunc
 */
class SymbolTableContext {
 public:
  SymbolTableContext() = default;
  ~SymbolTableContext() = default;
  // provide methods to manage the symbol table, such as adding variables/functions, looking up variables/functions,
  // etc.

  /**
   * @brief Enter a new scope
   */
  auto EnterScope() -> void { var_table_.emplace_back(); }

  /**
   * @brief Exit the current scope
   */
  auto ExitScope() -> void {
    if (!var_table_.empty()) {
      var_table_.pop_back();
    }
    // Should never exit the global scope
    throw std::logic_error("Cannot exit global scope");
  }

  /**
   * @brief Check if a variable is defined in the current scope
   * @param name the name of the variable
   * @return true if the variable is defined in the current scope, false otherwise
   */
  auto ExistsInCurrentScope(const std::string &name) -> bool;

  /**
   * @brief Look up a variable in the symbol table
   * @param name the name of the variable
   * @return a pointer to the variable entry if found, nullptr otherwise
   */
  auto LookupVar(const std::string &name) -> symboltable::VarEntry *;

  /**
   * @brief Look up a function in the symbol table
   * @param name the name of the function
   * @return a pointer to the function entry if found, nullptr otherwise
   */
  auto LookupFunc(const std::string &name) -> symboltable::FuncEntry *;

  /**
   * @brief Add a variable to the current scope
   * @param name the name of the variable
   * @param type the type of the variable
   * @param is_const whether the variable is const
   * @param dimensions the dimensions of the array if the variable is an array, otherwise it is empty
   * @return true if the variable is added successfully, false if the variable already exists in
   */
  auto AddVar(const std::string &name, type::Type *type, bool is_const = false,
              std::vector<std::size_t> dimensions = {}) -> bool;

  /**
   * @brief Add a function to the symbol table
   * @param name the name of the function
   * @param return_type the return type of the function
   * @param param_types the parameter types of the function
   * @return true if the function is added successfully, false otherwise
   */
  auto AddFunc(const std::string &name, type::Type *return_type, std::vector<type::Type *> param_types = {}) -> bool;

 private:
  std::vector<std::unordered_map<std::string, symboltable::VarEntry>> var_table_;
  std::unordered_map<std::string, symboltable::FuncEntry> func_table_;
};
}  // namespace context

#endif