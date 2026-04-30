#ifndef MIDDLEEND_MLIR_DIALECT_SCF_IF_H
#define MIDDLEEND_MLIR_DIALECT_SCF_IF_H
#include <stdexcept>
#include "context/mlir_context.h"
#include "middleend/mlir/dialect/scf/basic.h"
namespace middleend::mlir::scf {
class IfOp : public SCFOps {
 public:
  ~IfOp() override = default;

  static auto Create(context::MLIRContext *context, size_t num_results, mlir::Value *condition,
                     mlir::Region *then_region, mlir::Region *else_region = nullptr) -> IfOp * {
    auto op = new IfOp();
    op->SetUp(context, num_results, condition, then_region, else_region);
    return op;
  }

  auto Emit(std::ofstream *out) -> void override {
    PrintIndent(out);
    for (size_t i = 0; i < results_.size(); ++i) {
      results_[i]->Emit(out);
      if (i != results_.size() - 1) {
        *out << ", ";
      }
    }
    if (!results_.empty()) {
      *out << " = ";
    }
    *out << Name() << " ";
    operands_[0]->Emit(out);
    *out << " {\n";
    indent_level_++;
    regions_[0]->indent_level_ = indent_level_;
    regions_[0]->Emit(out);
    indent_level_--;
    PrintIndent(out);
    *out << "}";
    if (regions_.size() > 1) {
      *out << " else {\n";
      indent_level_++;
      regions_[1]->indent_level_ = indent_level_;
      regions_[1]->Emit(out);
      indent_level_--;
      PrintIndent(out);
      *out << "}";
    }
    *out << "\n";
  }

  auto GetCondition() -> mlir::Value * { return operands_[0]; }
  auto GetThenRegion() -> mlir::Region * { return regions_[0]; }
  auto GetElseRegion() -> mlir::Region * { return regions_.size() > 1 ? regions_[1] : nullptr; }
  /**
   * @brief Get the result of the if operation BY index
   */
  auto GetResultByIndex(size_t index) -> mlir::Value * {
    if (index >= results_.size()) {
      throw std::runtime_error("Index out of bounds when getting result from if operation.");
    }
    return results_[index];
  }

 private:
  IfOp() : SCFOps("if") {}
  auto SetUp(context::MLIRContext *context, size_t num_results, mlir::Value *condition, mlir::Region *then_region,
             mlir::Region *else_region) -> void {
    operands_.reserve(1);
    results_.reserve(num_results);
    for (size_t i = 0; i < num_results; ++i) {
      auto res = context->CreateValue();
      res->op_ = this;
      results_.emplace_back(res);
    }
    operands_.emplace_back(condition);
    regions_.emplace_back(then_region);
    if (else_region == nullptr) {
      // Currently we will set default yield in else region,
      // which means there should always be an else region,
      // even if there is no else statement in the source code.
      throw std::logic_error("Else region should not be null, since we will set default yield in else region.");
      return;
    }
    if (else_region != nullptr) {
      regions_.emplace_back(else_region);
    }
  }
};
}  // namespace middleend::mlir::scf

#endif