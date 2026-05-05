#ifndef MIDDLEEND_MLIR_DIALECT_BASE_UNARY_H
#define MIDDLEEND_MLIR_DIALECT_BASE_UNARY_H
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/base/basic.h"
#include "mlir/utils/value/value.h"
namespace middleend::mlir::base {
#include "middleend/mlir/dialect/base/def/intunaryop.def"

DEFINE_INTUNARY_OP(Neg, neg)

#include "middleend/mlir/dialect/base/def/floatunaryop.def"

DEFINE_FLOATUNARY_OP(NegF, negf)

// Not operation only accept boolean type,
// so we do not need to separate them into different operations based on the types.
/**
 * @brief Use a macro to define integer unary operations.
 */
class NotOp : public BaseOps {
 public:
  static auto Create(mlir::Value *res, mlir::Value *operand) -> NotOp * {
    auto op = new NotOp();
    op->SetUp(res, operand);
    return op;
  }
  auto GetOperand() -> mlir::Value * { return operands_[0]; }
  auto GetRes() -> mlir::Value * { return results_[0]; }
  auto Emit(std::ofstream *out) -> void override { /* Implementation for emitting the operation */
    PrintIndent(out);
    results_[0]->Emit(out);
    *out << " = " << Name() << " ";
    operands_[0]->Emit(out);
    *out << "\n";
  }

  auto Verify(context::CompilerContext *context) -> bool override {
    if (operands_.size() != 1 || results_.size() != 1) {
      context->Print("NotOp: operand or result size is not correct\n");
      return false;
    }
    if (results_[0] == nullptr) {
      context->Print("NotOp: result is null\n");
      return false;
    }
    if (operands_[0] == nullptr) {
      context->Print("NotOp: operand is null\n");
      return false;
    }
    if (operands_[0]->GetType()->GetKind() != type::TypeKind::BOOL) {
      context->Print("NotOp: operand type is not boolean\n");
      return false;
    }
    if (results_[0]->GetType() == nullptr) {
      context->Print("NotOp: result type is nullptr\n");
      return false;
    }
    return true;
  }

 private:
  NotOp() : BaseOps("not") {}
  auto SetUp(mlir::Value *res, mlir::Value *operand) -> void {
    /* NotOp has one operand and one result */
    operands_.reserve(1);
    results_.reserve(1);
    operands_.emplace_back(operand);
    results_.emplace_back(res);
    res->op_ = this;
  }
};

// Handling const op, separate them into different operations according to the type
class ConstIntOp : public BaseOps {
 public:
  ~ConstIntOp() override = default;
  static auto Create(mlir::Value *res, int value) {
    auto op = new ConstIntOp();
    op->SetUp(res, value);
    return op;
  }

  auto Emit(std::ofstream *out) -> void override {
    PrintIndent(out);
    results_[0]->Emit(out);
    *out << " = " << Name() << " ";
    *out << value_ << "\n";
  }
  auto Verify(context::CompilerContext *context) -> bool override {
    // ConstIntOp should have no operand and one result
    if (!operands_.empty()) {
      context->Print("ConstIntOp: operand size is not 0\n");
      return false;
    }
    if (results_.size() != 1) {
      context->Print("ConstIntOp: result size is not 1\n");
      return false;
    }
    if (results_[0]->GetType()->GetKind() != type::TypeKind::INT) {
      context->Print("ConstIntOp: result type is not int\n");
      return false;
    }
    return true;
  }

 private:
  explicit ConstIntOp() : BaseOps("constint") {}
  auto SetUp(mlir::Value *res, int value) -> void {
    results_.reserve(1);
    value_ = value;
    results_.emplace_back(res);
    res->op_ = this;
  }
  int value_;
};

class ConstFloatOp : public BaseOps {
 public:
  ~ConstFloatOp() override = default;
  static auto Create(mlir::Value *res, float value) {
    auto op = new ConstFloatOp();
    op->SetUp(res, value);
    return op;
  }

  auto Emit(std::ofstream *out) -> void override {
    PrintIndent(out);
    results_[0]->Emit(out);
    *out << " = " << Name() << " ";
    *out << value_ << "\n";
  }
  auto Verify(context::CompilerContext *context) -> bool override {
    // ConstFloatOp should have no operand and one result
    if (!operands_.empty()) {
      context->Print("ConstFloatOp: operand size is not 0\n");
      return false;
    }
    if (results_.size() != 1) {
      context->Print("ConstFloatOp: result size is not 1\n");
      return false;
    }
    if (results_[0]->GetType()->GetKind() != type::TypeKind::FLOAT) {
      context->Print("ConstFloatOp: result type is not float\n");
      return false;
    }
    return true;
  }

 private:
  explicit ConstFloatOp() : BaseOps("constfloat") {}
  auto SetUp(mlir::Value *res, float value) -> void {
    results_.reserve(1);
    value_ = value;
    results_.emplace_back(res);
    res->op_ = this;
  }
  float value_;
};
}  // namespace middleend::mlir::base

#endif