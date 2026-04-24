#ifndef MIDDLEEND_MLIR_DIALECT_VAR_DEF_H
#define MIDDLEEND_MLIR_DIALECT_VAR_DEF_H
#include <fstream>
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
  static auto Create(context::MLIRContext *context, std::vector<int> &dimensions, std::vector<Value *> &inits,
                     bool is_global = false, bool is_const = false) -> VarDefOp * {
    auto op = new VarDefOp();
    op->SetUp(context, dimensions, inits, is_global, is_const);
    return op;
  }

  auto GetInits() -> std::vector<Value *> & { return operands_; }
  auto GetRes() -> Value * { return results_[0]; }
  auto Emit(std::ofstream *out) const -> void override {
    /**
     * @example: %res[%dim1, %dim2, ...] = var.def %init1, %init2, ...
     */
    results_[0]->Emit(out);
    *out << "[";
    for (const auto &dim : dimensions_) {
      *out << dim;
      if (dim != dimensions_.back()) {
        *out << ", ";
      }
    }
    *out << "] = " << Name() << " ";
    for (const auto &init : operands_) {
      if (init != nullptr) {
        init->Emit(out);
      } else {
        *out << "default_init";
      }
      if (init != operands_.back()) {
        *out << ", ";
      }
    }
    *out << "\n";
  }

 private:
  VarDefOp() : VarOps("def") {}
  auto SetUp(context::MLIRContext *context, std::vector<int> &dimensions, std::vector<Value *> &inits, bool is_global,
             bool is_const) -> void {
    operands_.reserve(inits.size());
    dimensions_.reserve(dimensions.size());
    results_.reserve(1);
    operands_ = std::move(inits);
    dimensions_ = std::move(dimensions);
    auto res = context->CreateValue();
    results_.emplace_back(res);
    is_global_ = is_global;
    is_const_ = is_const;
  }
  bool is_global_;
  bool is_const_;
  std::vector<int> dimensions_;
};

}  // namespace middleend::mlir::var

#endif