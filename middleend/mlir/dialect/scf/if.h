#ifndef MIDDLEEND_MLIR_DIALECT_SCF_IF_H
#define MIDDLEEND_MLIR_DIALECT_SCF_IF_H
#include "context/mlir_context.h"
#include "middleend/mlir/dialect/scf/basic.h"
namespace middleend::mlir::scf {
class IfOp : public SCFOps {
 public:
  ~IfOp() override = default;

  static auto Create(context::MLIRContext *context, mlir::Value *condition, mlir::Region *then_region,
                     mlir::Region *else_region = nullptr) -> IfOp * {
    auto op = new IfOp();
    op->SetUp(context, condition, then_region, else_region);
    return op;
  }

  auto Emit(std::ofstream *out) const -> void override {
    *out << Name() << " ";
    operands_[0]->Emit(out);
    *out << " {\n";
    regions_[0]->Emit(out);
    *out << "}";
    if (regions_.size() > 1) {
      *out << " else {\n";
      regions_[1]->Emit(out);
      *out << "}";
    }
    *out << "\n";
  }

  auto GetCondition() -> mlir::Value * { return operands_[0]; }
  auto GetThenRegion() -> mlir::Region * { return regions_[0]; }
  auto GetElseRegion() -> mlir::Region * { return regions_.size() > 1 ? regions_[1] : nullptr; }

 private:
  IfOp() : SCFOps("if") {}
  auto SetUp([[maybe_unused]] context::MLIRContext *context, mlir::Value *condition, mlir::Region *then_region,
             mlir::Region *else_region) -> void {
    operands_.reserve(1);
    results_.reserve(1);
    auto res = context->CreateValue();
    results_.emplace_back(res);
    operands_.emplace_back(condition);
    regions_.emplace_back(then_region);
    then_region->AddBlock(new Block());
    if (else_region != nullptr) {
      regions_.emplace_back(else_region);
      else_region->AddBlock(new Block());
    }
  }
};
}  // namespace middleend::mlir::scf

#endif