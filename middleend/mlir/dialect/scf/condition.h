#ifndef MIDDLEEND_MLIR_DIALECT_SCF_CONDITION_H
#define MIDDLEEND_MLIR_DIALECT_SCF_CONDITION_H
#include <cstddef>
#include <vector>
#include "compiler_context.h"
#include "context/mlir_context.h"
#include "middleend/mlir/dialect/scf/basic.h"
namespace middleend::mlir::scf {
class ConditionOp : public SCFOps {
 public:
  ~ConditionOp() override = default;

  static auto Create(context::MLIRContext *context, mlir::Value *cond_value) -> ConditionOp * {
    auto op = new ConditionOp();
    op->SetUp(context, cond_value);
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
      var_map_[entry]->AddUse(this, i + 1);
    }

    var_map_.clear();
  }

  auto Verify(context::CompilerContext *context) -> bool override;

 private:
  ConditionOp() : SCFOps("condition"){};
  auto SetUp(context::MLIRContext *context, mlir::Value *cond_value) -> void {
    // Record current variable map
    var_map_ = context->GetResourceContext()->GetVarMap();
    operands_.reserve(1);
    operands_.emplace_back(cond_value);
  }
  std::unordered_map<symboltable::VarEntry *, mlir::Value *> var_map_;
};

}  // namespace middleend::mlir::scf

#endif