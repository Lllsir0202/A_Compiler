#ifndef CONTEXT_SYMBOLTABLE_CONTEXT_H
#define CONTEXT_SYMBOLTABLE_CONTEXT_H
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include "utils/flatten/initializer.h"
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
  std::vector<int> dimensions_;  // for array variables, store the dimensions of the array

  // Check circular reference, used for semantic analysis
  bool is_being_defined_ = false;

  // the initializer of the variable, used for semantic analysis
  initializer::InitializerPtr initializer_ = nullptr;

  // Mark whether the variable is a global variable
  bool is_global_ = false;

  /**
   * Store the symbol id for the variable
   * Even though we can get the symbol id through the size of vector
   * I suppose it is more clear to generate the symbol id
   * when adding the entry to the symbol table.
   */
  size_t symbol_id_ = -1;
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
  /**
   * @brief Global variable table in the beginning.
   */
  SymbolTableContext() { var_table_.emplace_back(); }
  ~SymbolTableContext() {
    if (!var_table_.empty()) {
      var_table_.clear();
    }
    // Clear all resources in entries
    for (auto entry : entries_) {
      delete entry;
    }
  };
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
      return;
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
   * @param initializer the initializer of the variable, used for semantic analysis
   * @return true if the variable is added successfully, false if the variable already exists.
   */
  auto AddVar(const std::string &name, type::Type *type = nullptr, bool is_const = false,
              std::vector<int> dimensions = {}) -> bool;

  /**
   * @brief Set the members of a variable entry in the symbol table
   * @param name the name of the variable
   * @param type the type of the variable
   * @param is_const whether the variable is const
   * @param dimensions the dimensions of the array if the variable is an array, otherwise it is empty
   * @param initializer the initializer of the variable, used for semantic analysis
   * @return true if the variable entry is updated successfully, false if the variable does not exist in the symbol
   * table
   * @note this method will only update the members in current scope.
   * Used in Semantic Analysis
   */
  auto SetType(const std::string &name, type::Type *type) -> bool;
  auto SetConst(const std::string &name, bool is_const) -> bool;
  auto SetDimensions(const std::string &name, std::vector<int> dimensions) -> bool;
  auto SetInitializer(const std::string &name, initializer::ArrayInitializer initializer) -> bool;

  /**
   * @brief Add a function to the symbol table
   * @param name the name of the function
   * @param return_type the return type of the function
   * @param param_types the parameter types of the function
   * @return true if the function is added successfully, false otherwise
   */
  auto AddFunc(const std::string &name, type::Type *return_type, std::vector<type::Type *> param_types = {}) -> bool;

  /**
   * @brief Add a function parameter to the symbol
   * @param func_entry the function entry of the function parameter belongs to
   * @param name the name of the function parameter
   * @param type the type of the function parameter
   * @param dimensions the dimensions of the parameter if it is an array parameter, otherwise it is empty
   * @return true if the function parameter is added successfully, false otherwise
   */
  auto AddFuncParam(symboltable::FuncEntry *func_entry, const std::string &name, type::Type *type,
                    std::vector<int> dimensions = {}) -> bool;

  /**
   * @brief Reset the current function being processed.
   * @note Should only be called when the semantic analysis has terminaled, since it means there is no
   function being processed.
   */
  auto ResetCurrentFunc() -> void { current_func_ = nullptr; }

  /**
   * @brief Get the current function being processed.
   * @return a pointer to the current function entry if there is a function being processed,
   * @note nullptr might be, but normally it won't be a nullptr when calling this method
   */
  auto GetCurrentFunc() -> symboltable::FuncEntry * { return current_func_; }

  // Following methods are used for intermediate representation,
  // since we need to get the information of the global variables
  // and the function parameters when generating the IR.
  // and we have checked the semantic correctness, so it is safe to directly
  // generate the global variables

  /**
   * @brief Get the global variable table
   */
  auto GetGlobalVars() -> std::unordered_map<std::string, symboltable::VarEntry *> & {
    if (var_table_.empty()) {
      throw std::logic_error("Variable table should never be empty");
    }
    return var_table_.front();
  }

  /**
   * @brief Get the function entry table
   */
  auto GetFuncs() -> std::unordered_map<std::string, symboltable::FuncEntry> & { return func_table_; }

  /**
   * @brief Get the variable entries via symbol id, used in IR phase
   */
  auto GetVarEntryById(size_t symbol_id) -> symboltable::VarEntry * {
    if (symbol_id >= entries_.size()) {
      throw std::logic_error("Invalid symbol id");
    }
    return entries_[symbol_id];
  }

 private:
  std::vector<std::unordered_map<std::string, symboltable::VarEntry *>> var_table_;
  // We need to consider the function's parameters
  // Since they are always defined in the function scope and can be accessed by any part of the
  // function. It is just defined in function definition
  std::unordered_map<symboltable::FuncEntry *, std::unordered_map<std::string, symboltable::VarEntry>>
      func_param_table_;
  std::unordered_map<std::string, symboltable::FuncEntry> func_table_;
  // Keep track of the current function being processed, used for adding function parameters
  // When the semantic analyser reach the end, we will reset the member because it means the analysis
  // has terminaled.
  symboltable::FuncEntry *current_func_ = nullptr;

  /**
   * Followings are the methods used in IR phase.
   */

  // Record the variable entries to be reused.
  std::vector<symboltable::VarEntry *> entries_;
};
}  // namespace context

#endif