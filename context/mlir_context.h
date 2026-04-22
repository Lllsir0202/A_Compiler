#ifndef CONTEXT_MLIR_CONTEXT_H
#define CONTEXT_MLIR_CONTEXT_H
#include <cstddef>
#include <unordered_map>
#include <vector>
#include "middleend/mlir/utils/value/value.h"
#include "mlir/utils/operation/operation.h"
#include "mlir/utils/struct/block.h"
#include "symboltable_context.h"

namespace context {
/**
 * @brief This class is used to create and manage value
 * as well as creating operations
 */
class MLIRResourceContext {
 public:
  MLIRResourceContext() = default;
  ~MLIRResourceContext() {
    for (auto value : values_) {
      delete value;
    }
  }
  /**
   * @brief Provide an interface to create a value
   */
  auto CreateValue() -> middleend::mlir::Value * {
    auto value = middleend::mlir::Value::Create();
    value->num_ = value_num_++;
    values_.emplace_back(value);
    return value;
  }

  /**
   * @brief Get a new operation
   */
  template <typename OperationType, typename... Args>
  auto CreateOperation(Args &&...args) -> OperationType * {
    auto operation = OperationType::Create(std::forward<Args>(args)...);
    return operation;
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
  size_t value_num_ = 0;                          // for generating unique value numbers, i.e. %0, %1, etc.
  std::unordered_map<std::string, size_t> name_count_map_;
  // map from variable name to the num,
  // used for distinguishing different variables with the same name
  std::unordered_map<symboltable::VarEntry *, middleend::mlir::Value *>
      var_map_;  // map from variable entries to values
};

/**
 * @brief This class is used to record build state of the MLIR,
 * such as the current block for inserting operations, etc.
 */
class MLIRStateContext {
 public:
  auto SetInsertPoint(middleend::mlir::Block *block) -> void { current_block_ = block; }
  auto GetInsertPoint() -> middleend::mlir::Block * { return current_block_; }
  auto PushValue(middleend::mlir::Value *value) -> void { value_stack_.emplace_back(value); }
  auto PopValue() -> middleend::mlir::Value * {
    if (value_stack_.empty()) {
      throw std::runtime_error("Value stack is empty.");
    }
    auto value = value_stack_.back();
    value_stack_.pop_back();
    return value;
  }

 private:
  middleend::mlir::Block *current_block_ = nullptr;  // the current block for inserting operations
  /**
   * @brief The value stack is used for building expressions,
   * since the building of expressions is usually in post-order traversal,
   * we can push the values of the sub-expressions into the stack,
   * and then pop them when we need to build the parent expression.
   * @example
   * For example, for the expression "a + b * c",
   * we will first visit the sub-expression "b * c",
   * and push the value of "b" and "c" into the stack,
   * and then we will visit the parent expression "a + (b * c)",
   * and pop the value of "b" and "c" from the stack to build the value of "b * c", and then push the value of "b * c"
   * into the stack, and then we will pop the value of "a" and "b * c" from the stack to build the value of "a + (b *
   * c)", and then push the value of "a + (b * c)" into the stack.
   * @note
   * Since the value stack is only used for building expressions,
   * we can just put it in the state context, and it will be automatically cleared after building an expression.
   */
  std::vector<middleend::mlir::Value *> value_stack_;

  /**
   * @brief need to store the information for control flow building,
   * @example
   * i.e.
   * while(cond) {
   *   // loop body
   * }
   * need: iteration variable,
   */
};

/**
 * @brief Since the IR can be totally different among different IRs,
 * we will provide a context for the IR,
 * which will be used to manage the resources of the IR,
 */
class MLIRContext {
 public:
  MLIRContext() {
    resource_context_ = new MLIRResourceContext();
    state_context_ = new MLIRStateContext();
  }
  ~MLIRContext() {
    delete resource_context_;
    delete state_context_;
    // Since the module operation will not be destroyed by other operations,
    // we shall call delete while the MLIRContext is destroyed,
    // so that the memory of the module operation can be released.
    delete module_op_;
  }
  /**
   * @brief Get the resource context of the MLIR, which can be used to create values and operations
   */
  auto GetResourceContext() -> MLIRResourceContext * { return resource_context_; }

  /**
   * @brief Get the state context of the MLIR, which can be used to manage the build state of the MLIR
   */
  auto GetStateContext() -> MLIRStateContext * { return state_context_; }

  /**
   * @brief Provide interfaces to directly create or get something
   * it would be too too too long to write
   * `compiler_context_->GetIRContext()->GetMLIRContext()->GetResourceContext()->CreateValue()`
   */
  auto CreateValue() -> middleend::mlir::Value * { return resource_context_->CreateValue(); }

  template <typename OperationType, typename... Args>
  auto CreateOperation(Args &&...args) -> OperationType * {
    return resource_context_->CreateOperation<OperationType>(std::forward<Args>(args)...);
  }

  auto GetModuleOp() -> middleend::mlir::Operation * {
    if (module_op_ == nullptr) {
      throw std::runtime_error("Module operation has not been created yet.");
    }
    return module_op_;
  }

  auto SetModuleOp(middleend::mlir::Operation *module_op) -> void { module_op_ = module_op; }

  auto SetInsertPoint(middleend::mlir::Block *block) -> void { state_context_->SetInsertPoint(block); }
  auto GetInsertPoint() -> middleend::mlir::Block * { return state_context_->GetInsertPoint(); }

 private:
  MLIRResourceContext *resource_context_;
  MLIRStateContext *state_context_;
  // the module operation of the MLIR, which is the top-level operation
  middleend::mlir::Operation *module_op_ = nullptr;
};

}  // namespace context

#endif