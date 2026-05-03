#ifndef MIDDLEEND_MLIR_DIALECT_BASE_BRANCH_H
#define MIDDLEEND_MLIR_DIALECT_BASE_BRANCH_H
#include "middleend/mlir/dialect/base/basic.h"
namespace middleend::mlir::base {
// Unconditional Branch Operation
class BrOp : public BaseOps {
 public:
  ~BrOp() override = default;

  // Provide two methods to create BrOp,
  // we allow creating BrOp without target block at first,
  // and set the target block later.
  static auto Create(mlir::Block *target) -> BrOp * {
    auto *op = new BrOp();
    op->SetUp(target);
    return op;
  }

  static auto Create() -> BrOp * {
    auto *op = new BrOp();
    return op;
  }

  auto Emit(std::ofstream *out) -> void override;

  auto Verify(context::CompilerContext *context) -> bool override;

  auto GetTarget() -> mlir::Block * { return target_; }
  auto SetTarget(mlir::Block *target) -> void { target_ = target; }

 private:
  BrOp() : BaseOps("br") {}
  auto SetUp(mlir::Block *target) -> void { target_ = target; }
  mlir::Block *target_;
};

// Conditional Branch Operation
class CondBrOp : public BaseOps {
 public:
  ~CondBrOp() override = default;

  static auto Create(mlir::Value *condition, mlir::Block *true_target, mlir::Block *false_target) -> CondBrOp * {
    auto *op = new CondBrOp();
    op->SetUp(condition, true_target, false_target);
    return op;
  }

  // Allow creating CondBrOp without targets at first, and set the targets later.
  static auto Create(mlir::Value *condition) -> CondBrOp * {
    auto *op = new CondBrOp();
    op->SetUp(condition, nullptr, nullptr);
    return op;
  }

  auto Emit(std::ofstream *out) -> void override;

  auto Verify(context::CompilerContext *context) -> bool override;
  // Provide interfaces to get and set the true and false targets
  auto GetTrueTarget() -> mlir::Block * { return true_target_; }
  auto SetTrueTarget(mlir::Block *true_target) -> void { true_target_ = true_target; }
  auto GetFalseTarget() -> mlir::Block * { return false_target_; }
  auto SetFalseTarget(mlir::Block *false_target) -> void { false_target_ = false_target; }

  // Provide an interface to get the condition operand
  auto GetCondition() -> mlir::Value * { return operands_[0]; }

 private:
  CondBrOp() : BaseOps("cond_br") {}
  auto SetUp(mlir::Value *condition, mlir::Block *true_target, mlir::Block *false_target) -> void {
    operands_.reserve(1);
    operands_.emplace_back(condition);
    true_target_ = true_target;
    false_target_ = false_target;
  }

  mlir::Block *true_target_;
  mlir::Block *false_target_;
};

}  // namespace middleend::mlir::base

#endif