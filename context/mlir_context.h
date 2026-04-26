#ifndef CONTEXT_MLIR_CONTEXT_H
#define CONTEXT_MLIR_CONTEXT_H
#include <cstddef>
#include <unordered_map>
#include <vector>
#include "ast/ast_expr.h"
#include "middleend/mlir/utils/value/value.h"
#include "mlir/utils/operation/operation.h"
#include "mlir/utils/struct/block.h"
#include "mlir/utils/struct/region.h"
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

  /**
   * @brief Map an expression node to a value
   */
  auto MapExprToValue(frontend::ast::ExprNode *expr_node, middleend::mlir::Value *value) -> void {
    expr_map_[expr_node] = value;
  }

  /**
   * @brief Get the value mapped to an expression node
   */
  auto GetValueFromExpr(frontend::ast::ExprNode *expr_node) -> middleend::mlir::Value * {
    if (expr_node == nullptr) {
      return nullptr;
    }
    if (expr_map_.count(expr_node) > 0) {
      return expr_map_[expr_node];
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

  /**
   * @brief This map is used to record the mapping from expression nodes to values
   */
  std::unordered_map<frontend::ast::ExprNode *, middleend::mlir::Value *> expr_map_;
};

/**
 * @brief This class is used to record build state of the MLIR,
 * such as the current block for inserting operations, etc.
 */
class MLIRStateContext {
 public:
  /**
   * @brief Interface to set and get the current block for inserting operations.
   */
  auto SetInsertPoint(middleend::mlir::Block *block) -> void { current_block_ = block; }
  auto GetInsertPoint() -> middleend::mlir::Block * { return current_block_; }

  /**
   * @brief Interface to set and get the current region for inserting blocks.
   */
  auto SetCurrentRegion(middleend::mlir::Region *region) -> void { current_region_ = region; }
  auto GetCurrentRegion() -> middleend::mlir::Region * { return current_region_; }

 private:
  middleend::mlir::Block *current_block_ = nullptr;  // the current block for inserting operations

  middleend::mlir::Region *current_region_ =
      nullptr;  // the current region for inserting blocks, which is used for control flow building

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

  /**
   * @brief Quick interfaces to set and get
   */
  auto SetInsertPoint(middleend::mlir::Block *block) -> void { state_context_->SetInsertPoint(block); }
  auto GetInsertPoint() -> middleend::mlir::Block * { return state_context_->GetInsertPoint(); }
  auto SetCurrentRegion(middleend::mlir::Region *region) -> void { state_context_->SetCurrentRegion(region); }
  auto GetCurrentRegion() -> middleend::mlir::Region * { return state_context_->GetCurrentRegion(); }

 private:
  MLIRResourceContext *resource_context_;
  MLIRStateContext *state_context_;
  // the module operation of the MLIR, which is the top-level operation
  middleend::mlir::Operation *module_op_ = nullptr;
};

}  // namespace context

#endif