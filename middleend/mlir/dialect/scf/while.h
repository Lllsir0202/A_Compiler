#ifndef MIDDLEEND_MLIR_DIALECT_SCF_WHILE_H
#define MIDDLEEND_MLIR_DIALECT_SCF_WHILE_H

#include "context/mlir_context.h"
#include "middleend/mlir/dialect/scf/basic.h"
namespace middleend::mlir::scf {
class WhileOp : public SCFOps {
 public:
  ~WhileOp() override = default;

  static auto Create(context::MLIRContext *context, mlir::Region *cond_region, mlir::Region *body_region) -> WhileOp * {
    auto op = new WhileOp();
    op->SetUp(context, cond_region, body_region);
    return op;
  }

  auto Emit(std::ofstream *out) -> void override {
    PrintIndent(out);
    *out << Name() << " {\n";
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

 private:
  WhileOp() : SCFOps("while"){};
  auto SetUp([[maybe_unused]] context::MLIRContext *context, mlir::Region *cond_region,
             mlir::Region *body_region) -> void {
    regions_.emplace_back(cond_region);
    regions_.emplace_back(body_region);
  }
};

}  // namespace middleend::mlir::scf

#endif