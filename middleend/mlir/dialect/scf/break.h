#ifndef MIDDLEEND_MLIR_DIALECT_SCF_BREAK_H
#define MIDDLEEND_MLIR_DIALECT_SCF_BREAK_H

#include <cstddef>
#include "context/mlir_context.h"
#include "middleend/mlir/dialect/scf/basic.h"
namespace middleend::mlir::scf {
class BreakOp : public SCFOps {
 public:
  ~BreakOp() override = default;

  static auto Create(context::MLIRContext *context) -> BreakOp * {
    auto op = new BreakOp();
    op->SetUp(context);
    return op;
  }

  auto Emit(std::ofstream *out) -> void override {
    PrintIndent(out);
    *out << Name() << " ";
    for (size_t i = 0; i < operands_.size(); ++i) {
      operands_[i]->Emit(out);
      if (i != operands_.size() - 1) {
        *out << ", ";
      }
    }
    *out << "\n";
  }

  auto GetOperands() -> std::vector<mlir::Value *> { return operands_; }

  auto Verify(context::CompilerContext *context) -> bool override;

  // SetUp the loop-carried variables for the break operation
  auto SetLoopCarriedVars(const std::vector<symboltable::VarEntry *> &changed_vars) -> void {
    // Set them as the oeprands of the break operation
    auto size = changed_vars.size();
    operands_.reserve(size);

    // Get the value of the changed variables
    for (size_t i = 0; i < changed_vars.size(); ++i) {
      auto entry = changed_vars[i];
      operands_.emplace_back(var_map_[entry]);
      var_map_[entry]->AddUse(this, i);
    }
    // Once we set the operands, we can clear the variable map
    var_map_.clear();
  }

 private:
  BreakOp() : SCFOps("break"){};
  auto SetUp(context::MLIRContext *context) -> void { var_map_ = context->GetResourceContext()->GetVarMap(); }
  // We need to store a slapshot of the variable map when we create the break operation
  // Then when we finish generate the loop, we can get the loop-carried variables and set them to the break operation
  std::unordered_map<symboltable::VarEntry *, mlir::Value *> var_map_;
};

}  // namespace middleend::mlir::scf

#endif