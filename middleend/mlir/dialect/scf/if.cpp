#include "middleend/mlir/dialect/scf/if.h"
#include "context/compiler_context.h"
#include "mlir/dialect/scf/yield.h"

namespace middleend::mlir::scf {
auto IfOp::Verify(context::CompilerContext *context) -> bool {
  if (operands_[0]->GetType()->GetKind() != type::TypeKind::BOOL) {
    context->Print("IfOp: condition operand must be of boolean type\n");
    return false;
  }
  // Since we only allow if-else region, there should be always 2 regions
  if (regions_.size() != 2) {
    context->Print("IfOp: must have exactly 2 regions\n");
    return false;
  }
  auto then_region = regions_[0];
  auto else_region = regions_[1];
  // then_region and else_region should end with a yield operation
  if (DynamicOrNull<YieldOp>(then_region->blocks_[0]->operations_.back()) == nullptr) {
    context->Print("IfOp: then region must end with a yield operation\n");
    return false;
  }
  if (DynamicOrNull<YieldOp>(else_region->blocks_[0]->operations_.back()) == nullptr) {
    context->Print("IfOp: else region must end with a yield operation\n");
    return false;
  }
  for (const auto &op : then_region->blocks_[0]->operations_) {
    if (!op->Verify(context)) {
      context->Print("IfOp: operation in then region failed verification\n");
      return false;
    }
  }
  for (const auto &op : else_region->blocks_[0]->operations_) {
    if (!op->Verify(context)) {
      context->Print("IfOp: operation in else region failed verification\n");
      return false;
    }
  }
  return true;
}

}  // namespace middleend::mlir::scf