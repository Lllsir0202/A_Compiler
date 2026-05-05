#ifndef MIDDLEEND_MLIR_DIALECT_SCF_CONTINUE_H
#define MIDDLEEND_MLIR_DIALECT_SCF_CONTINUE_H
#include "compiler_context.h"
#include "context/mlir_context.h"
#include "middleend/mlir/dialect/scf/basic.h"
namespace middleend::mlir::scf {
class ContinueOp : public SCFOps {
 public:
  ~ContinueOp() override = default;

  static auto Create(context::MLIRContext *context) -> ContinueOp * {
    auto op = new ContinueOp();
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

  // Only thing from break operation, continue operation will jump to the condition block
  auto SetLoopCarriedVars(const std::vector<symboltable::VarEntry *> &changed_vars) -> void {
    // Set them as the oeprands of the continue operation
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

  auto Verify(context::CompilerContext *context) -> bool override;

 private:
  ContinueOp() : SCFOps("continue"){};
  auto SetUp(context::MLIRContext *context) -> void { var_map_ = context->GetResourceContext()->GetVarMap(); }
  // We need to store a slapshot of the variable map when we create the continue operation
  // Then when we finish generate the loop, we can get the loop-carried variables and set them to the continue operation
  std::unordered_map<symboltable::VarEntry *, mlir::Value *> var_map_;
};
}  // namespace middleend::mlir::scf

#endif