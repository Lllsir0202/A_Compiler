#ifndef MIDDLEEND_MLIR_DIALECT_BASE_COPY_H
#define MIDDLEEND_MLIR_DIALECT_BASE_COPY_H

#include "middleend/mlir/dialect/base/basic.h"

namespace middleend::mlir::base {
class CopyOp : public BaseOps {
 public:
  ~CopyOp() override = default;
  static auto Create(context::MLIRContext *context, mlir::Value *input) {
    auto op = new CopyOp();
    op->SetUp(context, input);
    return op;
  }

  auto Emit(std::ofstream *out) -> void override;
  auto Verify(context::CompilerContext *context) -> bool override;

 private:
  explicit CopyOp() : BaseOps("copy") {}
  auto SetUp(context::MLIRContext *context, mlir::Value *input) -> void;
};

}  // namespace middleend::mlir::base

#endif