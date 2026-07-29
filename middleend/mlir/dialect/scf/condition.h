#ifndef MIDDLEEND_MLIR_DIALECT_SCF_CONDITION_H
#define MIDDLEEND_MLIR_DIALECT_SCF_CONDITION_H
#include <cstddef>
#include <unordered_map>
#include <vector>
#include "compiler_context.h"
#include "context/mlir_context.h"
#include "middleend/mlir/dialect/scf/basic.h"
#include "mlir/utils/adapter/adapter.h"
#include "mlir/utils/value/value.h"
#include "symboltable_context.h"
namespace middleend::mlir::scf {
class ConditionOp : public SCFOps {
 public:
  ~ConditionOp() override = default;

  static auto Create(context::MLIRContext *context, mlir::Value *cond_value,
                     std::unordered_map<symboltable::VarEntry *, mlir::Value *> original_value_map) -> ConditionOp * {
    auto op = new ConditionOp();
    op->SetUp(context, cond_value, original_value_map);
    return op;
  }

  auto Emit(std::ofstream *out) -> void override {
    PrintIndent(out);
    *out << Name() << " ";
    operands_[0]->Emit(out);
    *out << " : ";
    for (size_t i = 1; i < operands_.size(); ++i) {
      operands_[i]->Emit(out);
      if (i != operands_.size() - 1) {
        *out << ", ";
      }
    }
    *out << "\n";
  }

  // SetUp the loop-carried variables for the condition operation
  auto SetLoopCarriedVars(const std::vector<symboltable::VarEntry *> &changed_vars) -> void {
    // Set them as the oeprands of the condition operation
    // Get the value of the changed variables

    for (size_t i = 0; i < changed_vars.size(); ++i) {
      auto entry = changed_vars[i];
      operands_.emplace_back(var_map_[entry]);
      // Since the origin value is just a record for lowering
      // we don't need to add use for it.
      original_values_.emplace_back(original_var_map_[entry]);
      var_map_[entry]->AddUse(this, i + 1);
    }

    var_map_.clear();
    original_var_map_.clear();
  }

  auto SetLoopCarriedValues(const std::vector<mlir::Value *> &values, const std::vector<mlir::Value *> &original_values)
      -> void {
    operands_.resize(1);
    for (auto *value : values) {
      operands_.emplace_back(value);
    }
    original_values_ = original_values;
  }

  auto Verify(context::CompilerContext *context) -> bool override;

  /**
   * @brief Provide interface to get the operands
   */

  auto GetCondition() -> mlir::Value * { return operands_[0]; }
  auto GetOperands() -> std::vector<mlir::Value *> {
    std::vector<mlir::Value *> operands;
    for (size_t i = 1; i < operands_.size(); ++i) {
      operands.emplace_back(operands_[i]);
    }
    return operands;
  }

  auto GetOriginalValues() -> std::vector<mlir::Value *> { return original_values_; }

  auto UpdateOriginalValue(size_t index, mlir::Value *new_value) -> void {
    if (index >= original_values_.size()) {
      throw std::runtime_error("The index of the original value is out of range");
    }
    original_values_[index] = new_value;
  }

 private:
  ConditionOp() : SCFOps("condition") {};
  auto SetUp(context::MLIRContext *context, mlir::Value *cond_value,
             std::unordered_map<symboltable::VarEntry *, mlir::Value *> &original_value_map) -> void {
    // Record current variable map
    var_map_ = context->GetResourceContext()->GetVarMap();
    original_var_map_ = std::move(original_value_map);
    operands_.reserve(1);
    operands_.emplace_back(cond_value);
  }
  std::unordered_map<symboltable::VarEntry *, mlir::Value *> var_map_;
  std::unordered_map<symboltable::VarEntry *, mlir::Value *> original_var_map_;
  std::vector<mlir::Value *> original_values_;
};

}  // namespace middleend::mlir::scf

#endif
