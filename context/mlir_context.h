#ifndef CONTEXT_MLIR_CONTEXT_H
#define CONTEXT_MLIR_CONTEXT_H
#include <cstddef>
#include <unordered_map>
#include <vector>
#include "context/symboltable_context.h"
#include "context/type_context.h"
#include "frontend/ast/ast_expr.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/block.h"
#include "middleend/mlir/utils/struct/region.h"
#include "middleend/mlir/utils/value/value.h"

namespace middleend::mlir::func {
class FuncDefOp;
}  // namespace middleend::mlir::func
namespace middleend::mlir::arithm {
class ConstOp;
}  // namespace middleend::mlir::arithm
namespace middleend::mlir::scf {
class BreakOp;
class ContinueOp;
class WhileOp;
}  // namespace middleend::mlir::scf

namespace middleend::mlir::base {
class FuncDefOp;
}  // namespace middleend::mlir::base

namespace context {
/**
 * @brief This class is used to create and manage value
 * as well as creating operations
 */
class MLIRResourceContext {
 public:
  MLIRResourceContext() = default;
  ~MLIRResourceContext() {
    for (auto op : operations_) {
      delete op;
    }
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
    operations_.push_back(operation);
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
    value_num_--;  // Decrement the global value number since we are reusing the same variable name
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
   * @brief Get the entry-value mapping, used for if Operation
   */
  auto GetVarMap() -> std::unordered_map<symboltable::VarEntry *, middleend::mlir::Value *> { return var_map_; }

  /**
   * @brief Set the entry-value mapping, used for if Operation
   */
  auto SetVarMap(std::unordered_map<symboltable::VarEntry *, middleend::mlir::Value *> var_map) -> void {
    var_map_ = std::move(var_map);
  }

  /**
   * @brief Map an expression node to a value
   */
  auto MapExprToValue(frontend::ast::ExprNode *expr_node, middleend::mlir::Value *value) -> void {
    expr_map_[expr_node] = value;
  }

  /**
   * @brief Set the argument value name
   */
  auto SetArgValueName(middleend::mlir::Value *value) -> void {
    value->name_ = "arg";
    value->num_ = arg_num_++;
    value_num_--;  // Decrement the global value number since we are reusing the same variable name
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

  /**
   * @brief Set the mapping from function name to function definition operation.
   */
  auto MapFuncToFuncDefOp(const std::string &func_name, middleend::mlir::func::FuncDefOp *func_def_op) -> void {
    func_map_[func_name] = func_def_op;
  }

  /**
   * @brief Get the function definition operation mapped to a function name
   */
  auto GetFuncDefOpFromFuncName(const std::string &func_name) -> middleend::mlir::func::FuncDefOp * {
    if (func_map_.count(func_name) > 0) {
      return func_map_[func_name];
    }
    return nullptr;  // Not found
  }

  /**
   * @brief Get the constant operation from the string representation of the constant value
   */
  auto GetConstOpFromConstValue(const std::string &const_value) -> middleend::mlir::arithm::ConstOp * {
    if (const_map_.count(const_value) > 0) {
      return const_map_[const_value];
    }
    return nullptr;  // Not found
  }

  /**
   * @brief Set the constant operation for a constant value
   */
  auto SetConstOpForConstValue(const std::string &const_value, middleend::mlir::arithm::ConstOp *const_op) -> void {
    const_map_[const_value] = const_op;
  }

  /**
   * @brief Reset
   */
  auto Reset() -> void { const_map_.clear(); }

  /**
   * @breif Reset the function map when finishing the mlir visitor
   */
  auto ResetFuncMap() -> void { func_map_.clear(); }

  /**
   * @brief Enter a loop
   */
  auto EnterLoop() -> void {
    break_info_vec_.emplace_back();
    continue_info_vec_.emplace_back();
  }

  /**
   * @brief Exit a loop
   */
  auto ExitLoop() -> void {
    if (break_info_vec_.empty() || continue_info_vec_.empty()) {
      throw std::logic_error("No loop to exit.");
    }
    break_info_vec_.pop_back();
    continue_info_vec_.pop_back();
  }

  /**
   * @brief Map a loop operation to its corresponding break operation, used for break operation to find the
   * corresponding loop operation
   */
  auto AddBreakOp(middleend::mlir::scf::BreakOp *break_op) -> void {
    if (break_info_vec_.empty()) {
      throw std::logic_error("No loop to map the break operation to.");
    }
    break_info_vec_.back().emplace_back(break_op);
  }

  /**
   * @brief Get the break operation mapped to a loop operation
   */
  auto GetBreakOp() -> std::vector<middleend::mlir::scf::BreakOp *> {
    if (break_info_vec_.empty()) {
      throw std::logic_error("No loop to get the break operation from.");
    }
    return break_info_vec_.back();
  }

  /**
   * @brief Map a loop operation to its corresponding continue operation, used for continue operation to find the
   * corresponding loop operation
   */
  auto AddContinueOp(middleend::mlir::scf::ContinueOp *continue_op) -> void {
    if (continue_info_vec_.empty()) {
      throw std::logic_error("No loop to map the continue operation to.");
    }
    continue_info_vec_.back().emplace_back(continue_op);
  }

  /**
   * @brief Get the continue operation mapped to a loop operation
   */
  auto GetContinueOp() -> std::vector<middleend::mlir::scf::ContinueOp *> {
    if (continue_info_vec_.empty()) {
      throw std::logic_error("No loop to get the continue operation from.");
    }
    return continue_info_vec_.back();
  }

  /**
   * @brief Add a loop operation to the loop info vector, used for future optimization
   */
  auto AddLoop(middleend::mlir::scf::WhileOp *while_op) -> void { loop_info_vec_.emplace_back(while_op); }

  /**
   * @brief Provide interface to get loop
   */
  auto GetLoop() -> std::vector<middleend::mlir::scf::WhileOp *> { return loop_info_vec_; }

 private:
  // Manage all operations here
  std::vector<middleend::mlir::Operation *> operations_;  // all operations created by this builder, used for memory
                                                          // management

  std::vector<middleend::mlir::Value *> values_;  // all values created by this builder, used for memory management
  size_t value_num_ = 0;                          // for generating unique value numbers, i.e. %0, %1, etc.

  size_t arg_num_ = 0;  // for generating unique argument numbers, i.e. %arg0, %arg1, etc.
  std::unordered_map<std::string, size_t> name_count_map_;
  // map from variable name to the num,
  // used for distinguishing different variables with the same name
  std::unordered_map<symboltable::VarEntry *, middleend::mlir::Value *>
      var_map_;  // map from variable entries to values

  /**
   * @brief This map is used to record the mapping from expression nodes to values
   */
  std::unordered_map<frontend::ast::ExprNode *, middleend::mlir::Value *> expr_map_;

  /**
   * @brief Get the funcdef_op from the func_name(std:;string),
   * since we don't allow function overloading.
   */
  std::unordered_map<std::string, middleend::mlir::func::FuncDefOp *> func_map_;

  /**
   * @brief Since the constant operation can be reused, we can also store the mapping from constant value to the
   * constant operation
   * @note: since the constant value can be of different types, we can use the string representation of the constant
   * value as the key of the map, and then we can get the corresponding constant operation from the map, so that we
   * can reuse the constant operation if the same constant value appears again.
   */
  std::unordered_map<std::string, middleend::mlir::arithm::ConstOp *> const_map_;

  /**
   * @brief This vector is used to get break operations
   */
  std::vector<std::vector<middleend::mlir::scf::BreakOp *>> break_info_vec_;

  /**
   * @brief Vector for loop operations to get continue
   */
  std::vector<std::vector<middleend::mlir::scf::ContinueOp *>> continue_info_vec_;

  /**
   * @brief Record all loops, since it can be more convenient for future optimization
   */
  std::vector<middleend::mlir::scf::WhileOp *> loop_info_vec_;
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

  /**
   * @brief Interface to set and get the current function definition operation, which is used for control flow
   * building
   */
  auto SetCurrentFuncDefOp(middleend::mlir::func::FuncDefOp *func_def_op) -> void {
    current_func_def_op_ = func_def_op;
  }
  auto GetCurrentFuncDefOp() -> middleend::mlir::func::FuncDefOp * { return current_func_def_op_; }

 private:
  middleend::mlir::Block *current_block_ = nullptr;  // the current block for inserting operations

  middleend::mlir::Region *current_region_ =
      nullptr;  // the current region for inserting blocks, which is used for control flow building

  middleend::mlir::func::FuncDefOp *current_func_def_op_ =
      nullptr;  // the current function definition operation, which is used for control flow building

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
 * @brief This class is used for lowering the high-level IR to low-level IR
 */
class MLIRLowerContext {
 public:
  MLIRLowerContext() = default;
  ~MLIRLowerContext() = default;

  /**
   * @brief Provide interfaces to record and get the whileop->exit block
   */
  auto MapWhileOpToExitBlock(middleend::mlir::scf::WhileOp *while_op, middleend::mlir::Block *exit_block) -> void {
    while_2_exit_block_[while_op] = exit_block;
  }

  auto GetExitBlockFromWhileOp(middleend::mlir::scf::WhileOp *while_op) -> middleend::mlir::Block * {
    if (while_2_exit_block_.count(while_op) > 0) {
      return while_2_exit_block_[while_op];
    }
    return nullptr;  // Not found
  }

  auto MapWhileOpToCondBlock(middleend::mlir::scf::WhileOp *while_op, middleend::mlir::Block *cond_block) -> void {
    while_2_cond_block_[while_op] = cond_block;
  }

  auto GetCondBlockFromWhileOp(middleend::mlir::scf::WhileOp *while_op) -> middleend::mlir::Block * {
    if (while_2_cond_block_.count(while_op) > 0) {
      return while_2_cond_block_[while_op];
    }
    return nullptr;  // Not found
  }

  auto MapWhileOpToBodyBlock(middleend::mlir::scf::WhileOp *while_op, middleend::mlir::Block *body_block) -> void {
    while_2_body_block_[while_op] = body_block;
  }

  auto GetBodyBlockFromWhileOp(middleend::mlir::scf::WhileOp *while_op) -> middleend::mlir::Block * {
    if (while_2_body_block_.count(while_op) > 0) {
      return while_2_body_block_[while_op];
    }
    return nullptr;  // Not found
  }

  /**
   * @brief Record while loop and get which loop we are currently in
   */
  auto EnterWhileLoop(middleend::mlir::scf::WhileOp *while_op) -> void { while_op_vec_.emplace_back(while_op); }

  auto ExitWhileLoop() -> void {
    if (while_op_vec_.empty()) {
      throw std::logic_error("No while loop to exit.");
    }
    while_op_vec_.pop_back();
  }

  auto GetCurrentWhileLoop() -> middleend::mlir::scf::WhileOp * {
    if (while_op_vec_.empty()) {
      return nullptr;  // Not found
    }
    return while_op_vec_.back();
  }

  auto MapFuncNameToFuncDefOp(const std::string &func_name, middleend::mlir::base::FuncDefOp *func_def_op) -> void {
    func_name_2_func_def_op_[func_name] = func_def_op;
  }

  auto GetFuncDefOpFromFuncName(const std::string &func_name) -> middleend::mlir::base::FuncDefOp * {
    if (func_name_2_func_def_op_.count(func_name) > 0) {
      return func_name_2_func_def_op_[func_name];
    }
    return nullptr;  // Not found
  }

 private:
  // Record map from while operation to its corresponding exit block,
  // which is used for lowering break operation
  std::unordered_map<middleend::mlir::scf::WhileOp *, middleend::mlir::Block *> while_2_exit_block_;
  std::unordered_map<middleend::mlir::scf::WhileOp *, middleend::mlir::Block *> while_2_cond_block_;
  std::unordered_map<middleend::mlir::scf::WhileOp *, middleend::mlir::Block *> while_2_body_block_;

  std::vector<middleend::mlir::scf::WhileOp *> while_op_vec_;

  // Map from func name to base funcdef op,
  // used for func call operation to get
  // the corresponding function definition operation
  std::unordered_map<std::string, middleend::mlir::base::FuncDefOp *> func_name_2_func_def_op_;
};

/**
 * @brief Since the IR can be totally different among different IRs,
 * we will provide a context for the IR,
 * which will be used to manage the resources of the IR,
 */
class MLIRContext {
 public:
  explicit MLIRContext(TypeContext *type_context) {
    resource_context_ = new MLIRResourceContext();
    state_context_ = new MLIRStateContext();
    lower_context_ = new MLIRLowerContext();
    type_context_ = type_context;
  }
  ~MLIRContext() {
    delete resource_context_;
    delete state_context_;
    delete lower_context_;
    // Since the module operation will not be destroyed by other operations,
    // we shall call delete while the MLIRContext is destroyed,
    // so that the memory of the module operation can be released.
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
   * @brief Get the lower context of the MLIR, which can be used to manage the lowering state of the MLIR
   */
  auto GetLowerContext() -> MLIRLowerContext * { return lower_context_; }

  // Register Library Functions
  auto RegisterLibraryFunction() -> void;

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

  auto GetModuleOp() -> middleend::mlir::ModuleOp * {
    if (module_op_ == nullptr) {
      throw std::runtime_error("Module operation has not been created yet.");
    }
    return module_op_;
  }

  auto SetModuleOp(middleend::mlir::ModuleOp *module_op) -> void {
    // Set a new module operation, and delete the old one if it exists to avoid memory leak
    if (module_op_ == module_op) {
      // If the module operation is the same, no need to change.
      return;
    }
    if (module_op_ != nullptr) {
      module_op_->Reset();
    }
    module_op_ = module_op;
  }

  /**
   * @brief Quick interfaces to set and get
   */
  auto SetInsertPoint(middleend::mlir::Block *block) -> void { state_context_->SetInsertPoint(block); }
  auto GetInsertPoint() -> middleend::mlir::Block * { return state_context_->GetInsertPoint(); }
  auto SetCurrentRegion(middleend::mlir::Region *region) -> void { state_context_->SetCurrentRegion(region); }
  auto GetCurrentRegion() -> middleend::mlir::Region * { return state_context_->GetCurrentRegion(); }
  auto SetCurrentFuncDefOp(middleend::mlir::func::FuncDefOp *func_def_op) -> void {
    state_context_->SetCurrentFuncDefOp(func_def_op);
  }
  auto GetCurrentFuncDefOp() -> middleend::mlir::func::FuncDefOp * { return state_context_->GetCurrentFuncDefOp(); }

 private:
  MLIRResourceContext *resource_context_;
  MLIRStateContext *state_context_;
  MLIRLowerContext *lower_context_;

  // Type context
  TypeContext *type_context_;

  // the module operation of the MLIR, which is the top-level operation
  middleend::mlir::ModuleOp *module_op_ = nullptr;
};

}  // namespace context

#endif