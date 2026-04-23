#ifndef MIDDLEEND_MLIR_DIALECT_ARITHM_UNARYOP_H
#define MIDDLEEND_MLIR_DIALECT_ARITHM_UNARYOP_H
#include "middleend/builder/mlir/mlirbuilder.h"
#include "middleend/mlir/dialect/arithm/basic.h"
#include "middleend/mlir/utils/value/value.h"
#include "type/type.h"

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
  static auto Create(MLIRBuilder *builder, mlir::Value *value) {
    auto op = new ConstOp();
    op->SetUp(builder, value);
    return op;
  }
  /**
   * Since we might need to use and change the value and result of the operation
   * we should not make them const
   */
  auto GetValue() -> mlir::Value * { return value_; }
  auto GetResult() -> mlir::Value * { return res_; }

  auto Emit(std::ofstream *out) const -> void override {
    res_->Emit(out);
    *out << " = " << Name() << " ";
    value_->Emit(out);
    *out << "\n";
  }

 private:
  ConstOp() : ArithmOps("const") {}
  auto SetUp(MLIRBuilder *builder, mlir::Value *value) -> void {
    operands_.reserve(1);
    results_.reserve(1);
    operands_.emplace_back(value);
    value_ = value;
    auto res = builder->GetMLIRContext()->CreateValue();
    results_.emplace_back(res);
    res_ = res;
  };
  mlir::Value *value_;
  mlir::Value *res_;
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
  static auto Create(MLIRBuilder *builder, mlir::Value *operand, type::Type *from, type::Type *to) {
    auto op = new CastOp();
    op->SetUp(builder, operand, from, to);
    return op;
  }
  auto GetOperand() -> mlir::Value * { return operand_; }
  auto GetResult() -> mlir::Value * { return res_; }
  auto GetFromType() -> type::Type * { return from_type_; }
  auto GetToType() -> type::Type * { return to_type_; }

  auto Emit(std::ofstream *out) const -> void override {
    res_->Emit(out);
    *out << " = " << Name() << " ";
    operand_->Emit(out);
    *out << " : " << from_type_->ToString() << " to " << to_type_->ToString() << "\n";
  };

 private:
  CastOp() : ArithmOps("cast") {}
  auto SetUp(MLIRBuilder *builder, mlir::Value *operand, type::Type *from, type::Type *to) -> void {
    operands_.reserve(1);
    results_.reserve(1);
    operands_.emplace_back(operand);
    operand_ = operand;
    from_type_ = from;
    to_type_ = to;
    auto res = builder->GetMLIRContext()->CreateValue();
    results_.emplace_back(res);
    res_ = res;
  }
  mlir::Value *operand_;
  mlir::Value *res_;
  type::Type *from_type_;
  type::Type *to_type_;
};

}  // namespace middleend::mlir::arithm

#endif