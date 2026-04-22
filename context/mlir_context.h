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
    var_map_[var_entry] = value;
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
  std::unordered_map<symboltable::VarEntry *, size_t>
      value_num_map_;  // map from variable entries to value numbers, used for printing
  std::unordered_map<symboltable::VarEntry *, middleend::mlir::Value *>
      var_map_;  // map from variable entries to values
};

}  // namespace context

#endif