#ifndef MIDDLEEND_MLIR_DIALECT_FUNC_FUNC_DEF_H
#define MIDDLEEND_MLIR_DIALECT_FUNC_FUNC_DEF_H

#include "context/mlir_context.h"
#include "middleend/mlir/dialect/func/basic.h"
#include "mlir/utils/struct/region.h"
#include "mlir/utils/value/value.h"
namespace middleend::mlir::func {
class FuncDefOp : public FuncOps {
 public:
  static auto Create(context::MLIRContext *context, const std::string &name, std::vector<mlir::Value *> &args,
                     type::Type *return_type) -> FuncDefOp * {
    auto func_def_op = new FuncDefOp();
    func_def_op->SetUp(context, name, args, return_type);
    return func_def_op;
  }

  auto Emit(std::ofstream *out) -> void override {
    PrintIndent(out);
    results_[0]->Emit(out);
    *out << " = " << Name() << "@" << name_ << "(";
    for (size_t i = 0; i < operands_.size(); ++i) {
      operands_[i]->Emit(out);
      if (i != operands_.size() - 1) {
        *out << ", ";
      }
    }
    *out << ") {\n";
    IncreaseIndent();
    // Emit the body of the function
    for (auto region : regions_) {
      region->indent_level_ = indent_level_;
      region->Emit(out);
    }
    DecreaseIndent();
    PrintIndent(out);
    *out << "}\n";
  }

  /**
   * @brief Interface to get the name of the function
   */
  auto GetName() const -> std::string { return name_; }

  /**
   * @brief Interface to get the return type of the function
   */
  auto GetReturnType() const -> type::Type * { return results_[0]->GetType(); }

  /**
   * @brief Interface to get the arguments of the function
   */
  auto GetArguments() const -> std::vector<mlir::Value *> { return operands_; }

  /**
   * @brief Interface to get the return value
   */
  auto GetRes() -> mlir::Value * { return results_[0]; }

 private:
  FuncDefOp() : FuncOps("def"){};
  auto SetUp(context::MLIRContext *context, const std::string &name, std::vector<mlir::Value *> &args,
             type::Type *return_type) -> void {
    // Generate the block and region
    regions_.emplace_back(new Region());
    regions_[0]->blocks_.emplace_back(new Block());
    name_ = name;
    operands_.reserve(args.size());
    results_.reserve(1);
    operands_ = std::move(args);
    auto res = context->CreateValue();
    res->SetType(return_type);
    results_.emplace_back(res);
    res->op_ = this;
  };
  std::string name_;
};

}  // namespace middleend::mlir::func

#endif