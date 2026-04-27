#ifndef MIDDLEEND_MLIR_DIALECT_ARITHM_UNARYOP_H
#define MIDDLEEND_MLIR_DIALECT_ARITHM_UNARYOP_H
#include <variant>
#include "context/mlir_context.h"
#include "middleend/mlir/dialect/arithm/basic.h"
#include "middleend/mlir/utils/value/value.h"
#include "mlir_context.h"

namespace middleend::mlir::arithm {

#include "middleend/mlir/dialect//arithm/unaryop.def"

DEFINE_UNARY_OP(Neg)
DEFINE_UNARY_OP(Not)

/**
 * @brief Represents a constant operation in the MLIR,
 * we need to setup its type in the building phase
 */
class ConstOp : public ArithmOps {
 public:
  static auto Create(context::MLIRContext *context, int value) {
    auto op = new ConstOp();
    op->SetUp(context, value);
    return op;
  }
  static auto Create(context::MLIRContext *context, float value) {
    auto op = new ConstOp();
    op->SetUp(context, value);
    return op;
  }
  /**
   * Since we might need to use and change the value and result of the operation
   * we should not make them const
   */
  auto GetValue() -> std::variant<int, float> { return const_value_; }
  auto GetResult() -> mlir::Value * { return results_[0]; }

  auto Emit(std::ofstream *out) -> void override {
    PrintIndent(out);
    results_[0]->Emit(out);
    *out << " = " << Name() << " ";
    if (std::holds_alternative<int>(const_value_)) {
      *out << std::get<int>(const_value_);
    } else if (std::holds_alternative<float>(const_value_)) {
      *out << std::get<float>(const_value_);
    }
    *out << "\n";
  }

 private:
  ConstOp() : ArithmOps("const") {}
  auto SetUp(context::MLIRContext *context, int value) -> void {
    const_value_ = value;
    results_.reserve(1);
    auto res = context->CreateValue();
    results_.emplace_back(res);
    res->op_ = this;
  };
  auto SetUp(context::MLIRContext *context, float value) -> void {
    const_value_ = value;
    results_.reserve(1);
    auto res = context->CreateValue();
    results_.emplace_back(res);
    res->op_ = this;
  };
  ;
  std::variant<int, float> const_value_;
};

/**
 * @brief Represents a cast operation in the MLIR,
 * I suppose it can be used semantically, which means we don't want
 * it to be too specific, so I decide to make it a general cast operation,
 * which can be used for any type of cast.
 * After lowering, it will be transformed into specific cast operations,
 *such as int to float, float to int, etc.
 */
class CastOp : public ArithmOps {
 public:
  static auto Create(context::MLIRContext *context, mlir::Value *operand, type::Type *from, type::Type *to) {
    auto op = new CastOp();
    op->SetUp(context, operand, from, to);
    return op;
  }
  auto GetOperand() -> mlir::Value * { return operands_[0]; }
  auto GetRes() -> mlir::Value * { return results_[0]; }
  auto GetFromType() -> type::Type * { return from_type_; }
  auto GetToType() -> type::Type * { return to_type_; }

  auto Emit(std::ofstream *out) -> void override {
    PrintIndent(out);
    results_[0]->Emit(out);
    *out << " = " << Name() << " ";
    operands_[0]->Emit(out);
    *out << " : " << from_type_->ToString() << " to " << to_type_->ToString() << "\n";
  };

 private:
  CastOp() : ArithmOps("cast") {}
  auto SetUp(context::MLIRContext *context, mlir::Value *operand, type::Type *from, type::Type *to) -> void {
    operands_.reserve(1);
    results_.reserve(1);
    operands_.emplace_back(operand);
    from_type_ = from;
    to_type_ = to;
    auto res = context->CreateValue();
    results_.emplace_back(res);
    res->op_ = this;
    res->type_ = to;
  }
  type::Type *from_type_;
  type::Type *to_type_;
};
}  // namespace middleend::mlir::arithm

#endif