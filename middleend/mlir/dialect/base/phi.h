#ifndef MIDDLEEND_MLIR_DIALECT_BASE_PHI_H
#define MIDDLEEND_MLIR_DIALECT_BASE_PHI_H
#include "middleend/mlir/dialect/base/basic.h"
#include "mlir/utils/value/value.h"
namespace middleend::mlir::base {
class PhiOp : public BaseOps {
 public:
  ~PhiOp() override = default;

  static auto Create(mlir::Value *result, const std::vector<mlir::Value *> &values,
                     const std::vector<mlir::Block *> &blocks) -> PhiOp * {
    auto *op = new PhiOp();
    op->SetUp(result, values, blocks);
    return op;
  }

  // Allow creating PhiOp without incoming values at first,
  // and set the incoming values later.
  static auto Create(mlir::Value *result) -> PhiOp * {
    auto *op = new PhiOp();
    op->SetUp(result, {}, {});
    return op;
  }

  auto GetRes() -> mlir::Value * { return results_[0]; }

  // Get incoming pairs, and do not allow modifying.
  auto GetIncomingValue() const -> const std::vector<mlir::Value *> & { return operands_; }
  auto GetIncomingBlock() const -> const std::vector<mlir::Block *> & { return blocks_; }

  auto GetIncomingSize() const -> size_t { return operands_.size(); }

  // Add an incoming pair, which is used when creating PhiOp without incoming values at first.
  auto AddIncoming(mlir::Value *value, mlir::Block *block) -> void {
    operands_.emplace_back(value);
    blocks_.emplace_back(block);
  }

  auto UpdateIncoming(size_t index, mlir::Value *value, mlir::Block *block) -> void {
    if (index < operands_.size()) {
      operands_[index] = value;
      blocks_[index] = block;
    } else {
      throw std::out_of_range("Index out of range in UpdateIncoming");
    }
  }

  auto SetIncomings(const std::vector<mlir::Value *> &values, const std::vector<mlir::Block *> &blocks) -> void {
    operands_ = values;
    blocks_ = blocks;
  }

  auto Emit(std::ofstream *out) -> void override;

  auto Verify(context::CompilerContext *context) -> bool override;

 private:
  PhiOp() : BaseOps("phi") {}
  auto SetUp(mlir::Value *result, const std::vector<mlir::Value *> &values,
             const std::vector<mlir::Block *> &blocks) -> void {
    results_.reserve(1);
    results_.emplace_back(result);
    result->op_ = this;
    operands_.reserve(values.size());
    blocks_.reserve(values.size());
    for (size_t i = 0; i < values.size(); ++i) {
      operands_.emplace_back(values[i]);
      blocks_.emplace_back(blocks[i]);
    }
  }

  std::vector<mlir::Block *> blocks_;
};

}  // namespace middleend::mlir::base

#endif