#ifndef MIDDLEEND_MLIR_DIALECT_SCF_WHILE_H
#define MIDDLEEND_MLIR_DIALECT_SCF_WHILE_H

#include <stdexcept>
#include "context/compiler_context.h"
#include "context/mlir_context.h"
#include "middleend/mlir/dialect/scf/basic.h"
#include "middleend/mlir/utils/operation/operation.h"
namespace middleend::mlir::scf {
class WhileOp : public SCFOps {
 public:
  ~WhileOp() override = default;

  static auto Create(context::MLIRContext *context, size_t num_results, mlir::Region *cond_region,
                     mlir::Region *body_region) -> WhileOp * {
    auto op = new WhileOp();
    op->SetUp(context, num_results, cond_region, body_region);
    return op;
  }

  auto Emit(std::ofstream *out) -> void override {
    PrintIndent(out);
    for (size_t i = 0; i < results_.size(); ++i) {
      results_[i]->Emit(out);
      *out << " : " << results_[i]->GetType()->ToString();
      if (i != results_.size() - 1) {
        *out << ", ";
      }
    }
    if (!results_.empty()) {
      *out << " = " << Name() << " {\n";
    } else {
      *out << Name() << " {\n";
    }
    indent_level_++;
    regions_[0]->indent_level_ = indent_level_;
    regions_[0]->Emit(out);
    indent_level_--;
    PrintIndent(out);
    *out << "} do {\n";
    indent_level_++;
    regions_[1]->indent_level_ = indent_level_;
    regions_[1]->Emit(out);
    indent_level_--;
    PrintIndent(out);
    *out << "}\n";
  }

  auto Verify(context::CompilerContext *context) -> bool override;
  // Get the condition region
  auto GetCondRegion() -> mlir::Region * { return regions_[0]; }

  // Get the body region
  auto GetBodyRegion() -> mlir::Region * { return regions_[1]; }

  // Get the results of the while operation by index
  auto GetResultByIndex(size_t index) -> mlir::Value * {
    if (index >= results_.size()) {
      throw std::logic_error("Index out of bounds when getting result from while operation.");
    }
    return results_[index];
  }

  auto GetResultsSize() -> size_t { return results_.size(); }

  /**
   * @brief Override virtual funciton Walk
   */
  auto Walk(Adapter *adapter) -> void override;

 private:
  WhileOp() : SCFOps("while"){};
  auto SetUp(context::MLIRContext *context, size_t num_results, mlir::Region *cond_region,
             mlir::Region *body_region) -> void {
    regions_.emplace_back(cond_region);
    regions_.emplace_back(body_region);
    results_.reserve(num_results);
    for (size_t i = 0; i < num_results; ++i) {
      auto value = context->CreateValue();
      results_.emplace_back(value);
      value->op_ = this;
    }
  }
};

}  // namespace middleend::mlir::scf

#endif