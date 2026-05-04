#ifndef MIDDLEEND_MLIR_DIALECT_SCF_INITS_H
#define MIDDLEEND_MLIR_DIALECT_SCF_INITS_H

#include "mlir/dialect/scf/basic.h"
namespace middleend::mlir::scf {
class InitsOp : public SCFOps {
 public:
  ~InitsOp() override = default;

  static auto Create(context::MLIRContext *context, std::vector<Value *> operands) -> InitsOp * {
    auto op = new InitsOp();
    op->SetUp(context, operands);
    return op;
  }

  auto Emit(std::ofstream *out) -> void override;

  auto Verify(context::CompilerContext *context) -> bool override;

  /**
   * @brief Interface to get operands and results
   */
  auto GetOperands() -> std::vector<Value *> { return operands_; }
  auto GetResById(size_t index) -> Value * {
    if (index >= results_.size()) {
      return nullptr;
    }
    return results_[index];
  }

 private:
  InitsOp() : SCFOps("inits"){};
  auto SetUp(context::MLIRContext *context, std::vector<Value *> &operands) -> void;
};

}  // namespace middleend::mlir::scf

#endif