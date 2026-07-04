#include "middleend/mlir/utils/clone/uselistrebuilder.h"
#include <cstddef>
#include "middleend/mlir/dialect/func/funcdef.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/block.h"
#include "middleend/mlir/utils/struct/region.h"
#include "middleend/mlir/utils/value/value.h"

namespace middleend::mlir::utils {
namespace {

auto ClearUsesInRegion(mlir::Region *region) -> void {
  if (region == nullptr) {
    return;
  }
  for (auto *block : region->blocks_) {
    for (auto *argument : block->arguments_) {
      if (argument != nullptr) {
        argument->ClearUses();
      }
    }
    for (auto *operation : block->operations_) {
      for (auto *result : operation->results_) {
        if (result != nullptr) {
          result->ClearUses();
        }
      }
      for (auto *nested_region : operation->regions_) {
        ClearUsesInRegion(nested_region);
      }
    }
  }
}

auto RebuildOperationUses(mlir::Operation *operation) -> void {
  for (size_t i = 0; i < operation->operands_.size(); ++i) {
    auto *operand = operation->operands_[i];
    if (operand != nullptr) {
      operand->AddUse(operation, i);
    }
  }
  for (auto *region : operation->regions_) {
    for (auto *block : region->blocks_) {
      for (auto *nested : block->operations_) {
        RebuildOperationUses(nested);
      }
    }
  }
}

}  // namespace

auto RebuildFunctionUses(func::FuncDefOp *func_op) -> void {
  // Phase 1: clear every use in the function before rebuilding any.
  for (auto *argument : func_op->GetArguments()) {
    if (argument != nullptr) {
      argument->ClearUses();
    }
  }
  for (auto *result : func_op->results_) {
    if (result != nullptr) {
      result->ClearUses();
    }
  }
  for (auto *region : func_op->regions_) {
    ClearUsesInRegion(region);
  }
  // Phase 2: rebuild operand uses now that all use-lists start empty.
  for (auto *region : func_op->regions_) {
    for (auto *block : region->blocks_) {
      for (auto *operation : block->operations_) {
        RebuildOperationUses(operation);
      }
    }
  }
}

}  // namespace middleend::mlir::utils
