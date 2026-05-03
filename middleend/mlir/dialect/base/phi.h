#ifndef MIDDLEEND_MLIR_DIALECT_BASE_PHI_H
#define MIDDLEEND_MLIR_DIALECT_BASE_PHI_H
#include "middleend/mlir/dialect/base/basic.h"
namespace middleend::mlir::base {
class PhiOp : public BaseOps {
 public:
  ~PhiOp() override = default;

  static auto Create(mlir::Value *result,
                     const std::vector<std::pair<mlir::Value *, mlir::Block *>> &incoming) -> PhiOp * {
    auto *op = new PhiOp();
    op->SetUp(result, incoming);
    return op;
  }

  // Allow creating PhiOp without incoming values at first,
  // and set the incoming values later.
  static auto Create(mlir::Value *result) -> PhiOp * {
    auto *op = new PhiOp();
    op->SetUp(result, {});
    return op;
  }

  auto GetResult() -> mlir::Value * { return results_[0]; }

  // Get incoming pairs, and do not allow modifying.
  auto GetIncoming() -> const std::vector<std::pair<mlir::Value *, mlir::Block *>> & { return incoming_; }

  // Add an incoming pair, which is used when creating PhiOp without incoming values at first.
  auto AddIncoming(mlir::Value *value, mlir::Block *block) -> void { incoming_.emplace_back(value, block); }

  auto UpdateIncoming(size_t index, mlir::Value *value, mlir::Block *block) -> void {
    if (index < incoming_.size()) {
      incoming_[index] = std::make_pair(value, block);
    } else {
      throw std::out_of_range("Index out of range in UpdateIncoming");
    }
  }

  auto Emit(std::ofstream *out) -> void override;

  auto Verify(context::CompilerContext *context) -> bool override;

 private:
  PhiOp() : BaseOps("phi") {}
  auto SetUp(mlir::Value *result, const std::vector<std::pair<mlir::Value *, mlir::Block *>> &incoming) -> void {
    results_.reserve(1);
    results_.emplace_back(result);
    incoming_ = incoming;
  }
  std::vector<std::pair<mlir::Value *, mlir::Block *>> incoming_;
};

}  // namespace middleend::mlir::base

#endif