#ifndef MIDDLEEND_MLIR_DIALECT_FUNC_RETURN_H
#define MIDDLEEND_MLIR_DIALECT_FUNC_RETURN_H
#include "context/mlir_context.h"
#include "middleend/mlir/dialect/func/basic.h"
#include "type/type.h"
namespace middleend::mlir::func {
class ReturnOp : public FuncOps {
 public:
  ~ReturnOp() override = default;

  static auto Create(context::MLIRContext *context, type::Type *return_type, mlir::Value *return_value) -> ReturnOp * {
    auto op = new ReturnOp();
    op->SetUp(context, return_type, return_value);
    return op;
  }

  static auto Create(context::MLIRContext *context, type::Type *return_type) -> ReturnOp * {
    auto op = new ReturnOp();
    op->SetUp(context, return_type);
    return op;
  }

  // Used in lowering
  static auto Create(ReturnOp *other) -> ReturnOp * {
    auto op = new ReturnOp();

    if (other->GetReturnValue() != nullptr) {
      op->SetUp(other->GetRes(), other->GetReturnValue()->GetType(), other->GetReturnValue());
      other->GetReturnValue()->ReplaceAllOpUsesWith(other, op);
    } else {
      op->SetUp(other->GetRes(), other->GetRes()->GetType());
    }
    return op;
  }

  auto Emit(std::ofstream *out) -> void override {
    PrintIndent(out);
    *out << Name();
    if (!operands_.empty()) {
      *out << " ";
      operands_[0]->Emit(out);
      // print return type
      *out << " : " << operands_[0]->GetType()->ToString();
    }
    *out << "\n";
  }

  auto Verify(context::CompilerContext *context) -> bool override;

  /**
   * @brief Get the return value of the return operation, if there is no return value, return nullptr
   */
  auto GetReturnValue() -> mlir::Value * { return operands_.empty() ? nullptr : operands_[0]; }

  /**
   * @brief Get Result of the return operation
   */
  auto GetRes() -> mlir::Value * { return results_[0]; }

 private:
  ReturnOp() : FuncOps("return") {}
  auto SetUp(context::MLIRContext *context, type::Type *return_type, mlir::Value *return_value) -> void {
    operands_.reserve(1);
    results_.reserve(1);
    operands_.emplace_back(return_value);
    auto res = context->CreateValue();
    res->SetType(return_type);
    results_.emplace_back(res);
    res->op_ = this;
  }

  auto SetUp(context::MLIRContext *context, type::Type *return_type) -> void {
    results_.reserve(1);
    auto res = context->CreateValue();
    res->SetType(return_type);
    results_.emplace_back(res);
    res->op_ = this;
  }

  //  Used in lowering
  auto SetUp(mlir::Value *res, type::Type *return_type, mlir::Value *return_value) -> void {
    operands_.reserve(1);
    results_.reserve(1);
    if (return_value != nullptr) {
      // if return_value is null, it means there is no return value
      operands_.emplace_back(return_value);
    }
    results_.emplace_back(res);
    res->SetType(return_type);
    res->op_ = this;
  }

  auto SetUp(mlir::Value *res, type::Type *return_type) -> void {
    results_.reserve(1);
    results_.emplace_back(res);
    res->SetType(return_type);
    res->op_ = this;
  }
};
}  // namespace middleend::mlir::func

#endif