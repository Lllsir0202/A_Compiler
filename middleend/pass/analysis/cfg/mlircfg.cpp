#include "middleend/pass/analysis/cfg/mlircfg.h"
#include <queue>
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/base/branch.h"
#include "middleend/mlir/dialect/base/funcdef.h"
#include "middleend/mlir/utils/struct/block.h"
#include "mlir/dialect/func/return.h"
#include "mlir/utils/operation/operation.h"
#define GET_MLIR_CONTEXT() compiler_context_->GetIRContext()->GetMLIRContext()
namespace middleend::pass::analysis {
#include "middleend/checklibrary.ck"
auto MLIRCFGBuilder::BuildCFG() -> void {
  Reset();
  auto mlir_module = GET_MLIR_CONTEXT()->GetModuleOp();
  auto block = mlir_module->GetBlock();
  for (auto op : block->operations_) {
    if (auto func_def_op = mlir::DynamicOrNull<mlir::base::FuncDefOp>(op)) {
      if (!IsLibraryFunction(func_def_op->GetName())) {
        BuildFunctionCFG(func_def_op);
      }
    }
  }
}

auto MLIRCFGBuilder::BuildFunctionCFG(mlir::base::FuncDefOp *func_op) -> void {
  auto func_cfg = new MLIRFunctionCFG(func_op);
  function_cfgs_.emplace_back(func_cfg);
  auto entry_block = func_op->GetEntryBlock();
  auto entry_node = new MLIRCFGNode(entry_block);
  func_cfg->AddNode(entry_node);
  func_cfg->MapBlockToNode(entry_block, entry_node);
  func_cfg->SetEntryNode(entry_node);
  // Use a worklist to build the CFG, starting from the entry block
  std::queue<MLIRCFGNode *> worklist;
  worklist.push(entry_node);
  std::unordered_set<mlir::Block *> visited;
  visited.insert(entry_block);
  while (!worklist.empty()) {
    auto current_node = worklist.front();
    worklist.pop();
    // it represents a dfs naturally.
    func_cfg->PushDFSOrder(current_node);
    auto current_block = current_node->GetBlock();
    // Get the terminator operation of the current block, and find its successor blocks
    for (auto op : current_block->operations_) {
      if (auto br_op = mlir::DynamicOrNull<mlir::base::BrOp>(op)) {
        auto target_block = br_op->GetTarget();
        if (visited.count(target_block) == 0) {
          auto target_node = new MLIRCFGNode(target_block);
          func_cfg->AddNode(target_node);
          worklist.push(target_node);
          visited.insert(target_block);
          func_cfg->MapBlockToNode(target_block, target_node);
        }
        // Add successor and predecessor
        auto target_node = func_cfg->GetNodeByBlock(target_block);
        current_node->AddSuccessor(target_node);
        target_node->AddPredecessor(current_node);
        break;
      }
      if (auto cond_br_op = mlir::DynamicOrNull<mlir::base::CondBrOp>(op)) {
        auto true_target = cond_br_op->GetTrueTarget();
        auto false_target = cond_br_op->GetFalseTarget();
        if (visited.count(true_target) == 0) {
          auto true_node = new MLIRCFGNode(true_target);
          func_cfg->AddNode(true_node);
          worklist.push(true_node);
          visited.insert(true_target);
          func_cfg->MapBlockToNode(true_target, true_node);
        }
        if (visited.count(false_target) == 0) {
          auto false_node = new MLIRCFGNode(false_target);
          func_cfg->AddNode(false_node);
          worklist.push(false_node);
          visited.insert(false_target);
          func_cfg->MapBlockToNode(false_target, false_node);
        }
        // Add successors and predecessors for both true and false targets
        auto true_node = func_cfg->GetNodeByBlock(true_target);
        auto false_node = func_cfg->GetNodeByBlock(false_target);
        current_node->AddSuccessor(true_node);
        current_node->AddSuccessor(false_node);
        true_node->AddPredecessor(current_node);
        false_node->AddPredecessor(current_node);
        break;
      }
      if (auto ret_op = mlir::DynamicOrNull<mlir::func::ReturnOp>(op)) {
        func_cfg->AddExitNode(current_node);
        break;
      }
    }
  }
}
}  // namespace middleend::pass::analysis
