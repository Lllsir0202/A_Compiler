#ifndef MIDDLEEND_MLIR_DIALECT_BASE_FUNCCALL_H
#define MIDDLEEND_MLIR_DIALECT_BASE_FUNCCALL_H

#include "middleend/mlir/dialect/base/basic.h"
#include "mlir/utils/value/value.h"
namespace middleend::mlir::base {
class FuncCallOp : public BaseOps {
 public:
  // Used in lowering
  static auto Create(mlir::Value *res, const std::string &name,
                     const std::vector<mlir::Value *> &args) -> FuncCallOp * {
    auto func_call_op = new FuncCallOp();
    func_call_op->SetUp(res, name, args);
    return func_call_op;
  }

  auto Emit(std::ofstream *out) -> void override {
    PrintIndent(out);
    results_[0]->Emit(out);
    *out << " = " << Name() << "@" << name_ << "(";
    for (size_t i = 0; i < operands_.size(); ++i) {
      operands_[i]->Emit(out);
      *out << ": " << operands_[i]->GetType()->ToString();
      if (i != operands_.size() - 1) {
        *out << ", ";
      }
    }
    *out << ")\n";
  }

  auto Verify(context::CompilerContext *context) -> bool override;
  /**
   * @brief Get the result of the function call
   */
  auto GetRes() -> mlir::Value * { return results_[0]; }
  auto GetName() -> std::string { return name_; }
  /**
   * @brief Get the operands of the function call operation, which are the arguments of the function call
   */
  auto GetOperands() -> std::vector<mlir::Value *> { return operands_; }

 private:
  FuncCallOp() : BaseOps("call") {};

  // Used in lowering
  auto SetUp(mlir::Value *res, const std::string &name, std::vector<mlir::Value *> args) -> void {
    results_.reserve(1);
    operands_.reserve(args.size());
    name_ = name;
    operands_ = std::move(args);
    results_.emplace_back(res);
    res->op_ = this;
  };

  std::string name_;
};
}  // namespace middleend::mlir::base
#endif