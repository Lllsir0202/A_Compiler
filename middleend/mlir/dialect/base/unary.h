#ifndef MIDDLEEND_MLIR_DIALECT_BASE_UNARY_H
#define MIDDLEEND_MLIR_DIALECT_BASE_UNARY_H
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/base/basic.h"
namespace middleend::mlir::base {
#include "middleend/mlir/dialect/base/def/intunaryop.def"

DEFINE_INTUNARY_OP(Neg, neg)
DEFINE_INTUNARY_OP(Not, not )

#include "middleend/mlir/dialect/base/def/floatunaryop.def"

DEFINE_FLOATUNARY_OP(NegF, negf)
DEFINE_FLOATUNARY_OP(NotF, notf)

// Handling const op, separate them into different operations according to the type
class ConstIntOp : public BaseOps {
 public:
  ~ConstIntOp() override = default;
  static auto Create(context::MLIRContext *context, int value) {
    auto op = new ConstIntOp();
    op->SetUp(context, value);
    return op;
  }

  auto Emit(std::ofstream *out) -> void override {
    PrintIndent(out);
    results_[0]->Emit(out);
    *out << " = " << Name() << " ";
    *out << value_ << "\n";
    *out << "\n";
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
  auto SetUp(context::MLIRContext *context, int value) -> void {
    results_.reserve(1);
    value_ = value;
    auto res = context->CreateValue();
    results_.emplace_back(res);
    res->op_ = this;
  }
  int value_;
};

class ConstFloatOp : public BaseOps {
 public:
  ~ConstFloatOp() override = default;
  static auto Create(context::MLIRContext *context, float value) {
    auto op = new ConstFloatOp();
    op->SetUp(context, value);
    return op;
  }

  auto Emit(std::ofstream *out) -> void override {
    PrintIndent(out);
    results_[0]->Emit(out);
    *out << " = " << Name() << " ";
    *out << value_ << "\n";
    *out << "\n";
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
  auto SetUp(context::MLIRContext *context, float value) -> void {
    results_.reserve(1);
    value_ = value;
    auto res = context->CreateValue();
    results_.emplace_back(res);
    res->op_ = this;
  }
  float value_;
};
}  // namespace middleend::mlir::base

#endif