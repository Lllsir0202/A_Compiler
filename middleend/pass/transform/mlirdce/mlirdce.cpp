#include "middleend/pass/transform/mlirdce/mlirdce.h"
#include <deque>
#include <vector>
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/func/funccall.h"
#include "middleend/mlir/dialect/func/funcdef.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/block.h"
#include "middleend/mlir/utils/struct/region.h"

#define GET_MLIR_CONTEXT() compiler_context_->GetIRContext()->GetMLIRContext()

namespace middleend::pass::transform {
namespace {
auto IsLibraryFunctionName(const std::string &name) -> bool {
  return name == "getint" || name == "getch" || name == "getfloat" || name == "getarray" || name == "getfarray" ||
         name == "putint" || name == "putch" || name == "putfloat" || name == "putarray" || name == "putfarray" ||
         name == "_sysy_starttime" || name == "_sysy_stoptime";
}
}  // namespace

auto MLIRDeadFunctionEliminatePass::Run() -> void {
  CollectFunctions();
  MarkReachableFunctions();
  RemoveDeadFunctions();
}

auto MLIRDeadFunctionEliminatePass::CollectFunctions() -> void {
  function_map_.clear();
  reachable_functions_.clear();
  auto *module_op = GET_MLIR_CONTEXT()->GetModuleOp();
  if (module_op == nullptr) {
    throw std::logic_error("Module operation is not set in MLIRContext");
  }

  for (auto *operation : module_op->GetBlock()->operations_) {
    auto *func_op = mlir::DynamicOrNull<mlir::func::FuncDefOp>(operation);
    if (func_op == nullptr) {
      continue;
    }
    function_map_[func_op->GetName()] = func_op;
  }
}

auto MLIRDeadFunctionEliminatePass::IsRootFunction(const std::string &name) -> bool {
  return name == "main" || IsLibraryFunctionName(name);
}

auto MLIRDeadFunctionEliminatePass::MarkReachableFunctions() -> void {
  auto main_iter = function_map_.find("main");
  if (main_iter != function_map_.end()) {
    MarkReachableFrom(main_iter->second);
  }
}

auto MLIRDeadFunctionEliminatePass::MarkReachableFrom(mlir::func::FuncDefOp *func_op) -> void {
  if (func_op == nullptr || reachable_functions_.count(func_op) > 0) {
    return;
  }
  reachable_functions_.insert(func_op);
  if (IsLibraryFunctionName(func_op->GetName())) {
    return;
  }

  std::deque<mlir::Operation *> worklist;
  for (auto *operation : func_op->GetEntryBlock()->operations_) {
    worklist.emplace_back(operation);
  }

  while (!worklist.empty()) {
    auto *operation = worklist.front();
    worklist.pop_front();

    if (auto *call_op = mlir::DynamicOrNull<mlir::func::FuncCallOp>(operation)) {
      auto iter = function_map_.find(call_op->GetName());
      if (iter != function_map_.end()) {
        MarkReachableFrom(iter->second);
      }
    }

    for (auto *region : operation->regions_) {
      for (auto *block : region->blocks_) {
        for (auto *nested_op : block->operations_) {
          worklist.emplace_back(nested_op);
        }
      }
    }
  }
}

auto MLIRDeadFunctionEliminatePass::RemoveDeadFunctions() -> bool {
  auto *module_block = GET_MLIR_CONTEXT()->GetModuleOp()->GetBlock();
  bool changed = false;
  std::vector<mlir::Operation *> new_operations;
  new_operations.reserve(module_block->operations_.size());

  for (auto *operation : module_block->operations_) {
    auto *func_op = mlir::DynamicOrNull<mlir::func::FuncDefOp>(operation);
    if (func_op == nullptr || IsRootFunction(func_op->GetName()) || reachable_functions_.count(func_op) > 0) {
      new_operations.emplace_back(operation);
      continue;
    }
    changed = true;
  }

  module_block->operations_ = std::move(new_operations);
  return changed;
}

}  // namespace middleend::pass::transform
