#ifndef CONTEXT_MLIR_CONTEXT_H
#define CONTEXT_MLIR_CONTEXT_H
#include <cstddef>
#include <unordered_map>
#include <vector>
#include "middleend/mlir/utils/value/value.h"
#include "symboltable_context.h"

namespace context {
/**
 * @brief Since the IR can be totally different among different IRs,
 * we will provide a context for the IR,
 * which will be used to manage the resources of the IR,
 */
class MLIRContext {
 public:
  MLIRContext() = default;
  ~MLIRContext() {
    for (auto value : values_) {
      delete value;
    }
  }
  /**
   * @brief Provide an interface to create a value
   */
  auto CreateValue() -> middleend::mlir::Value * {
    auto value = middleend::mlir::Value::Create();
    values_.emplace_back(value);
    return value;
  }

  /**
   * @brief Map a variable entry to a value
   */
  auto MapVarToValue(symboltable::VarEntry *var_entry, middleend::mlir::Value *value) -> void {
    if (name_count_map_.count(var_entry->name_) == 0) {
      // New variable name, initialize the count to 0
      name_count_map_[var_entry->name_] = 0;
    } else {
      // Existing variable name, increment the count
      name_count_map_[var_entry->name_]++;
    }
    var_map_[var_entry] = value;
    value->num_ = name_count_map_[var_entry->name_];
    value->name_ = var_entry->name_;
  }

  /**
   * @brief Get the value mapped to a variable entry
   */
  auto GetValueFromVar(symboltable::VarEntry *var_entry) -> middleend::mlir::Value * {
    if (var_map_.count(var_entry) > 0) {
      return var_map_[var_entry];
    }
    return nullptr;  // Not found
  }

 private:
  std::vector<middleend::mlir::Value *> values_;  // all values created by this builder, used for memory management
  std::unordered_map<std::string, size_t> name_count_map_;
  // map from variable name to the num,
  // used for distinguishing different variables with the same name
  std::unordered_map<symboltable::VarEntry *, middleend::mlir::Value *>
      var_map_;  // map from variable entries to values
};

}  // namespace context

#endif