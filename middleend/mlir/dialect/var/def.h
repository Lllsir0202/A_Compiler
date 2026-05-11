#ifndef MIDDLEEND_MLIR_DIALECT_VAR_DEF_H
#define MIDDLEEND_MLIR_DIALECT_VAR_DEF_H
#include <cstddef>
#include <fstream>
#include "compiler_context.h"
#include "middleend/mlir/dialect/var/basic.h"
#include "mlir/utils/value/value.h"
#include "mlir_context.h"

namespace middleend::mlir::var {
/**
 * @brief Define a variable with a list of initializers
 * @param
 * - builder: MLIRBuilder
 * - inits: normalized linear initials
 * A definition operation's res should be mapped with a VarEntry(or the symbolId)
 * Thus the Lval can get the value of the variable by looking up the symbolId
 */
class VarDefOp : public VarOps {
 public:
  static auto Create(context::MLIRContext *context, std::vector<int> dimensions, std::vector<Value *> &inits,
                     bool is_const = false) -> VarDefOp * {
    auto op = new VarDefOp();
    op->SetUp(context, dimensions, inits, is_const);
    return op;
  }

  // Used in lowering when we need to create a new var def operation with specific result and operand
  static auto Create(VarDefOp *other) -> VarDefOp * {
    auto op = new VarDefOp();
    // Do not change other operation's attributes
    op->SetUp(other->results_[0], other->dimensions_, other->operands_, other->is_const_);
    for (auto &operand : other->operands_) {
      if (operand != nullptr) {
        operand->ReplaceAllOpUsesWith(other, op);
      }
    }
    return op;
  }

  auto GetInits() -> std::vector<Value *> & { return operands_; }
  auto GetRes() -> Value * { return results_[0]; }
  auto GetDimensions() const -> const std::vector<int> & { return dimensions_; }

  auto Emit(std::ofstream *out) -> void override {
    /**
     * @example: %res[%dim1, %dim2, ...] = var.def %init1, %init2, ...
     */
    PrintIndent(out);
    results_[0]->Emit(out);
    if (!dimensions_.empty()) {
      for (const auto &dim : dimensions_) {
        *out << "[" << dim << "]";
      }
    }
    *out << " " << results_[0]->GetType()->ToString();
    *out << " = ";
    if (is_const_) {
      *out << "const ";
    }
    *out << Name() << " " << "init: ";
    if (operands_.empty()) {
      *out << "default_init";
    }
    for (size_t i = 0; i < operands_.size(); ++i) {
      auto init = operands_[i];
      if (init != nullptr) {
        init->Emit(out);
      } else {
        *out << "d_i";
      }
      if (i != operands_.size() - 1) {
        *out << ", ";
      }
    }
    *out << "\n";
  }

  auto Verify(context::CompilerContext *context) -> bool override;

 private:
  VarDefOp() : VarOps("def") {}
  auto SetUp(context::MLIRContext *context, std::vector<int> &dimensions, std::vector<Value *> &inits,
             bool is_const) -> void {
    operands_.reserve(inits.size());
    dimensions_.reserve(dimensions.size());
    results_.reserve(1);
    operands_ = std::move(inits);
    dimensions_ = std::move(dimensions);
    auto res = context->CreateValue();
    results_.emplace_back(res);
    is_const_ = is_const;
    res->op_ = this;
  }

  auto SetUp(mlir::Value *res, std::vector<int> dimensions, std::vector<Value *> inits, bool is_const) -> void {
    operands_.reserve(inits.size());
    dimensions_.reserve(dimensions.size());
    results_.reserve(1);
    operands_ = std::move(inits);
    dimensions_ = std::move(dimensions);
    is_const_ = is_const;
    results_.emplace_back(res);
    res->op_ = this;
  }

  bool is_const_;
  std::vector<int> dimensions_;
};

}  // namespace middleend::mlir::var

#endif