#ifndef MIDDLEEND_MLIR_DIALECT_VAR_GLOBALDEF_CPP
#define MIDDLEEND_MLIR_DIALECT_VAR_GLOBALDEF_CPP
#include <utility>

#include "context/mlir_context.h"
#include "middleend/mlir/dialect/var/basic.h"

namespace middleend::mlir::var {
class GlobalVarDefOp : public VarOps {
 public:
  static auto Create(context::MLIRContext *context, std::string name, std::vector<int> dimensions,
                     std::vector<int> inits, bool is_const = false) -> GlobalVarDefOp * {
    auto op = new GlobalVarDefOp();
    op->SetUp(context, std::move(name), std::move(dimensions), std::move(inits), is_const);
    return op;
  }

  static auto Create(context::MLIRContext *context, std::string name, std::vector<int> dimensions,
                     std::vector<double> inits, bool is_const = false) -> GlobalVarDefOp * {
    auto op = new GlobalVarDefOp();
    op->SetUp(context, std::move(name), std::move(dimensions), std::move(inits), is_const);
    return op;
  }

  static auto Create(GlobalVarDefOp *other) -> GlobalVarDefOp * {
    auto op = new GlobalVarDefOp();
    // Do not change other operation's attributes
    op->SetUp(other->results_[0], other->name_, other->dimensions_, other->int_init_values_, other->float_init_values_,
              other->is_const_);
    for (auto &operand : other->operands_) {
      if (operand != nullptr) {
        operand->ReplaceAllOpUsesWith(other, op);
      }
    }
    return op;
  }

  auto Emit(std::ofstream *out) -> void override;
  auto Verify(context::CompilerContext *context) -> bool override;

  auto GetName() const -> const std::string & { return name_; }
  auto GetDimensions() const -> const std::vector<int> & { return dimensions_; }
  auto GetIntInitValues() const -> const std::vector<int> & { return int_init_values_; }
  auto GetFloatInitValues() const -> const std::vector<double> & { return float_init_values_; }
  auto IsConst() const -> bool { return is_const_; }
  auto GetRes() -> Value * { return results_[0]; }

 private:
  GlobalVarDefOp() : VarOps("globaldef") {}
  auto SetUp(context::MLIRContext *context, std::string name, std::vector<int> dimensions, std::vector<int> inits,
             bool is_const) -> void {
    name_ = std::move(name);
    results_.reserve(1);
    dimensions_ = std::move(dimensions);
    int_init_values_ = std::move(inits);
    is_const_ = is_const;
    auto res = context->CreateValue();
    results_.emplace_back(res);
    res->op_ = this;
  }

  auto SetUp(context::MLIRContext *context, std::string name, std::vector<int> dimensions, std::vector<double> inits,
             bool is_const) -> void {
    name_ = std::move(name);
    results_.reserve(1);
    dimensions_ = std::move(dimensions);
    float_init_values_ = std::move(inits);
    is_const_ = is_const;
    auto res = context->CreateValue();
    results_.emplace_back(res);
    res->op_ = this;
  }

  auto SetUp(mlir::Value *res, std::string name, std::vector<int> dimensions, std::vector<int> int_inits,
             std::vector<double> float_inits, bool is_const) -> void {
    name_ = std::move(name);
    results_.reserve(1);
    dimensions_ = std::move(dimensions);
    int_init_values_ = std::move(int_inits);
    float_init_values_ = std::move(float_inits);
    is_const_ = is_const;
    results_.emplace_back(res);
    res->op_ = this;
  }

  std::string name_;
  bool is_const_;
  std::vector<int> dimensions_;
  std::vector<int> int_init_values_;
  std::vector<double> float_init_values_;
};

}  // namespace middleend::mlir::var
#endif