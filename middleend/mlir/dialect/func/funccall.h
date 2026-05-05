#ifndef MIDDLEEND_MLIR_DIALECT_FUNC_FUNCCALL_H
#define MIDDLEEND_MLIR_DIALECT_FUNC_FUNCCALL_H
#include "context/mlir_context.h"
#include "middleend/mlir/dialect/func/basic.h"
namespace middleend::mlir::func {
class FuncCallOp : public FuncOps {
 public:
  static auto Create(context::MLIRContext *context, const std::string &name, std::vector<mlir::Value *> args,
                     type::Type *return_type) -> FuncCallOp * {
    auto func_call_op = new FuncCallOp();
    func_call_op->SetUp(context, name, args, return_type);
    return func_call_op;
  }

  // Used in lowering
  static auto Create(FuncCallOp *other) -> FuncCallOp * {
    auto func_call_op = new FuncCallOp();
    auto args = other->GetOperands();
    func_call_op->SetUp(other->GetRes(), other->GetName(), std::move(args));
    for (auto &operand : other->GetOperands()) {
      operand->ReplaceAllOpUsesWith(other, func_call_op);
    }
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
  FuncCallOp() : FuncOps("call"){};
  auto SetUp(context::MLIRContext *context, const std::string &name, std::vector<mlir::Value *> &args,
             type::Type *return_type) -> void {
    results_.reserve(1);
    operands_.reserve(args.size());
    name_ = name;
    operands_ = std::move(args);
    auto res = context->CreateValue();
    res->SetType(return_type);
    results_.emplace_back(res);
    res->op_ = this;
  };

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
}  // namespace middleend::mlir::func
#endif