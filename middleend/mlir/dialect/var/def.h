#ifndef MIDDLEEND_MLIR_DIALECT_VAR_DEF_H
#define MIDDLEEND_MLIR_DIALECT_VAR_DEF_H
#include <fstream>
#include "builder/mlir/mlirbuilder.h"
#include "middleend/mlir/dialect/var/basic.h"
#include "mlir/utils/value/value.h"

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
  static auto Create(MLIRBuilder *builder, std::vector<Value *> &dimensions, std::vector<Value *> &inits,
                     bool is_global = false) -> VarDefOp * {
    auto op = new VarDefOp();
    op->SetUp(builder, dimensions, inits, is_global);
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
      dim->Emit(out);
      if (dim != dimensions_.back()) {
        *out << ", ";
      }
    }
    *out << "] = " << Name() << " ";
    for (const auto &init : operands_) {
      init->Emit(out);
      if (init != operands_.back()) {
        *out << ", ";
      }
    }
    *out << "\n";
  }

 private:
  VarDefOp() : VarOps("def") {}
  auto SetUp(MLIRBuilder *builder, std::vector<Value *> &dimensions, std::vector<Value *> &inits,
             bool is_global) -> void {
    operands_.reserve(inits.size());
    dimensions_.reserve(dimensions.size());
    results_.reserve(1);
    operands_ = std::move(inits);
    dimensions_ = std::move(dimensions);
    auto res = builder->GetMLIRContext()->CreateValue();
    results_.emplace_back(res);
    is_global_ = is_global;
  }
  bool is_global_;
  std::vector<Value *> dimensions_;
};

}  // namespace middleend::mlir::var

#endif