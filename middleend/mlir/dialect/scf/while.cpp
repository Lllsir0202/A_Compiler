#include "middleend/mlir/dialect/scf/while.h"
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/scf/condition.h"
#include "middleend/mlir/dialect/scf/yield.h"

namespace middleend::mlir::scf {
auto WhileOp::Verify(context::CompilerContext *context) -> bool {
  if (regions_.size() != 2) {
    context->Print("WhileOp: must have exactly 2 regions\n");
    return false;
  }

  auto cond_region = regions_[0];
  auto body_region = regions_[1];

  // cond_end should be the terminator of the condition region, and it should be a condition operation
  auto cond_end = cond_region->blocks_[0]->operations_.back();
  if (DynamicOrNull<ConditionOp>(cond_end) == nullptr) {
    context->Print("WhileOp: condition region must end with a condition operation\n");
    return false;
  }
  // body_end should be the terminator of the body region, and it should be a yield operation
  auto body_end = body_region->blocks_[0]->operations_.back();
  if (DynamicOrNull<YieldOp>(body_end) == nullptr) {
    context->Print("WhileOp: body region must end with a yield operation\n");
    return false;
  }

  // Check all operations in the condition region
  for (const auto &op : cond_region->blocks_[0]->operations_) {
    if (!op->Verify(context)) {
      context->Print("WhileOp: operation in condition region failed verification\n");
      return false;
    }
  }
  // Check all operations in the body region
  for (const auto &op : body_region->blocks_[0]->operations_) {
    if (!op->Verify(context)) {
      context->Print("WhileOp: operation in body region failed verification\n");
      return false;
    }
  }
  return true;
}

auto WhileOp::Walk(Adapter *adapter) -> void {
  // Walk the condition region
  auto cond_region = GetCondRegion();
  for (const auto &op : cond_region->blocks_[0]->operations_) {
    op->Walk(adapter);
  }
  // Walk the body region
  auto body_region = GetBodyRegion();
  for (const auto &op : body_region->blocks_[0]->operations_) {
    op->Walk(adapter);
  }
}

}  // namespace middleend::mlir::scf