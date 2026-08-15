#include "middleend/pass/transform/divisibilitysimplify/divisibilitysimplify.h"
#include <cstdlib>
#include <iostream>
#include <vector>
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/base/binaryop.h"
#include "middleend/mlir/dialect/base/branch.h"
#include "middleend/mlir/dialect/base/unaryop.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/block.h"
#include "middleend/mlir/utils/value/value.h"
#include "middleend/pass/analysis/cfg/cfg.h"
#include "middleend/pass/analysis/cfg/mlircfg.h"

#define GET_MLIR_CONTEXT() compiler_context_->GetIRContext()->GetMLIRContext()
#define GET_PASS_CONTEXT() compiler_context_->GetPassContext()

namespace middleend::pass::transform {
namespace {

auto Constant(mlir::Value *value, int *result) -> bool {
  if (value == nullptr) {
    return false;
  }
  auto *constant = mlir::DynamicOrNull<mlir::base::ConstIntOp>(value->op_);
  if (constant == nullptr) {
    return false;
  }
  *result = constant->GetValue();
  return true;
}

auto PowerOfTwo(mlir::Value *value, int *constant, int *shift) -> bool {
  if (!Constant(value, constant) || *constant <= 0 ||
      (*constant & (*constant - 1)) != 0) {
    return false;
  }
  *shift = __builtin_ctz(static_cast<unsigned>(*constant));
  return true;
}

auto MatchesZero(mlir::Value *value) -> bool {
  int constant = 1;
  return Constant(value, &constant) && constant == 0;
}

auto DivisibilityCondition(mlir::Value *condition, mlir::Value *numerator,
                           int divisor, bool edge_is_true) -> bool {
  auto match_compare = [&](auto *compare, bool equality_when_true) {
    if (compare == nullptr) {
      return false;
    }
    bool edge_proves_equality =
        edge_is_true ? equality_when_true : !equality_when_true;
    if (!edge_proves_equality) {
      return false;
    }
    mlir::Value *remainder_value = nullptr;
    if (MatchesZero(compare->GetLHS())) {
      remainder_value = compare->GetRHS();
    } else if (MatchesZero(compare->GetRHS())) {
      remainder_value = compare->GetLHS();
    } else {
      return false;
    }
    auto *remainder =
        mlir::DynamicOrNull<mlir::base::ModOp>(remainder_value->op_);
    int remainder_divisor = 0;
    return remainder != nullptr && remainder->GetLHS() == numerator &&
           Constant(remainder->GetRHS(), &remainder_divisor) &&
           remainder_divisor == divisor;
  };
  if (match_compare(
          mlir::DynamicOrNull<mlir::base::EqualOp>(condition->op_), true)) {
    return true;
  }
  return match_compare(
      mlir::DynamicOrNull<mlir::base::NotEqualOp>(condition->op_), false);
}

}  // namespace

auto DivisibilitySimplifyPass::Run() -> void {
  PreRun();
  if (std::getenv("DIVISIBILITY_SIMPLIFY_OFF") != nullptr) {
    return;
  }
  candidates_ = 0;
  simplified_ = 0;
  auto *cfg_pass =
      GET_PASS_CONTEXT()->GetSpecPass<analysis::CFGPass>("CFGPass");
  if (cfg_pass == nullptr) {
    throw std::logic_error("DivisibilitySimplifyPass requires CFGPass");
  }
  for (auto *function_cfg : cfg_pass->Get()) {
    auto *function = dynamic_cast<analysis::MLIRFunctionCFG *>(function_cfg);
    if (function != nullptr) {
      RunOnFunction(function);
    }
  }
  if (std::getenv("LOOP_OPT_STATS") != nullptr) {
    std::cerr << "DivisibilitySimplify: candidates=" << candidates_
              << " simplified=" << simplified_ << '\n';
  }
}

auto DivisibilitySimplifyPass::RunOnFunction(
    analysis::MLIRFunctionCFG *function) -> bool {
  bool changed = false;
  for (auto *node : function->GetNodes()) {
    auto *mlir_node = dynamic_cast<analysis::MLIRCFGNode *>(node);
    if (mlir_node != nullptr) {
      changed = RunOnBlock(function, mlir_node->GetBlock()) || changed;
    }
  }
  return changed;
}

auto DivisibilitySimplifyPass::RunOnBlock(
    analysis::MLIRFunctionCFG *function, mlir::Block *block) -> bool {
  auto *node = function->GetNodeByBlock(block);
  if (node == nullptr || node->GetPredecessors().size() != 1) {
    return false;
  }
  auto *predecessor =
      dynamic_cast<analysis::MLIRCFGNode *>(node->GetPredecessors()[0]);
  if (predecessor == nullptr || predecessor->GetBlock()->operations_.empty()) {
    return false;
  }
  auto *branch = mlir::DynamicOrNull<mlir::base::CondBrOp>(
      predecessor->GetBlock()->operations_.back());
  if (branch == nullptr ||
      (branch->GetTrueTarget() != block &&
       branch->GetFalseTarget() != block)) {
    return false;
  }
  bool edge_is_true = branch->GetTrueTarget() == block;

  bool changed = false;
  std::vector<mlir::Operation *> operations;
  operations.reserve(block->operations_.size() + 2);
  for (auto *operation : block->operations_) {
    auto *divide = mlir::DynamicOrNull<mlir::base::DivOp>(operation);
    int divisor = 0;
    int shift = 0;
    if (divide == nullptr ||
        !PowerOfTwo(divide->GetRHS(), &divisor, &shift)) {
      operations.emplace_back(operation);
      continue;
    }
    ++candidates_;
    if (!DivisibilityCondition(branch->GetCondition(), divide->GetLHS(),
                               divisor, edge_is_true)) {
      operations.emplace_back(operation);
      continue;
    }

    auto *shift_value = GET_MLIR_CONTEXT()->CreateValue();
    shift_value->SetType(divide->GetRHS()->GetType());
    auto *shift_constant =
        mlir::base::ConstIntOp::Create(shift_value, shift);
    auto *arithmetic_shift =
        mlir::base::AShrOp::Create(divide->GetRes(), divide->GetLHS(),
                                  shift_value);
    divide->GetLHS()->RemoveUse(divide);
    divide->GetRHS()->RemoveUse(divide);
    divide->GetLHS()->AddUse(arithmetic_shift, 0);
    shift_value->AddUse(arithmetic_shift, 1);
    operations.emplace_back(shift_constant);
    operations.emplace_back(arithmetic_shift);
    ++simplified_;
    changed = true;
  }
  block->operations_ = std::move(operations);
  return changed;
}

}  // namespace middleend::pass::transform
