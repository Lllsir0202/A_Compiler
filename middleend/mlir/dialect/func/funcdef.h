#ifndef MIDDLEEND_MLIR_DIALECT_FUNC_FUNC_DEF_H
#define MIDDLEEND_MLIR_DIALECT_FUNC_FUNC_DEF_H

#include "context/mlir_context.h"
#include "middleend/mlir/dialect/func/basic.h"
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

  auto Emit(std::ofstream *out) const -> void override {
    *out << "func.func " << name_ << "(";
    for (size_t i = 0; i < args_.size(); ++i) {
      args_[i]->Emit(out);
      if (i != args_.size() - 1) {
        *out << ", ";
      }
    }
    *out << ") -> " << return_type_->ToString() << " {\n";
    // Emit the body of the function
    for (auto region : regions_) {
      region->Emit(out);
    }
    *out << "}\n";
  }

 private:
  FuncDefOp() : FuncOps("def"){};
  auto SetUp([[maybe_unused]] context::MLIRContext *context, const std::string &name, std::vector<mlir::Value *> &args,
             type::Type *return_type) -> void {
    name_ = name;
    args_ = std::move(args);
    return_type_ = return_type;
  };
  std::string name_;
  std::vector<mlir::Value *> args_;
  type::Type *return_type_;
};

}  // namespace middleend::mlir::func

#endif