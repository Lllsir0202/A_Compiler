#include "middleend/pass/transform/unify/unify.h"
#include <cstdio>
#include <string>
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/base/branch.h"
#include "middleend/mlir/dialect/base/funcdef.h"
#include "middleend/mlir/dialect/base/phi.h"
#include "middleend/mlir/dialect/func/return.h"
#include "middleend/mlir/utils/struct/block.h"
#include "middleend/pass/analysis/cfg/cfg.h"
#include "middleend/pass/analysis/cfg/mlircfg.h"

#define GET_MLIR_CONTEXT() compiler_context_->GetIRContext()->GetMLIRContext()
namespace middleend::pass::transform {
auto UnifyReturnPass::Run() -> void {
  try {
    PreRun();
  } catch (const std::exception &e) {
    // If PreRun throws an exception, we catch it and print the error message, then return
    compiler_context_->Print("Error in UnifyReturnPass PreRun: " + std::string(e.what()));
    throw std::logic_error("PreRun failed in UnifyReturnPass");
    return;
  }
  // Get the module operation
  auto module_op = GET_MLIR_CONTEXT()->GetModuleOp();
  if (module_op == nullptr) {
    throw std::logic_error("Module operation is not set in MLIRContext");
  }
  // For each function in the module, unify the return operation
  try {
    UnifyReturnInFunction();
  } catch (const std::exception &e) {
    compiler_context_->Print("Error in UnifyReturnInFunction: " + std::string(e.what()));
    throw std::logic_error("UnifyReturnInFunction failed in UnifyReturnPass");
    return;
  }
  compiler_context_->GetPassContext()->InvalidateAnalysis();
}

auto UnifyReturnPass::UnifyReturnInFunction() -> void {
  auto cfgpass = compiler_context_->GetPassContext()->GetSpecPass<analysis::CFGPass>("CFGPass");
  if (cfgpass == nullptr) {
    throw std::logic_error("CFGPass is not found in PassContext\n");
  }
  // Get the CFG of the function
  auto func_cfgs = cfgpass->Get();
  for (const auto &func_cfg : func_cfgs) {
    // Process each function CFG
    auto mlir_func_cfg = dynamic_cast<analysis::MLIRFunctionCFG *>(func_cfg);
    if (mlir_func_cfg == nullptr) {
      throw std::logic_error("Function CFG is not of type MLIRFunctionCFG\n");
    }
    auto func_op = mlir_func_cfg->GetFuncOp();
    if (func_op == nullptr) {
      throw std::logic_error("Function operation in MLIRFunctionCFG is null\n");
    }
    // Get exit nodes
    auto exit_nodes = func_cfg->GetExitNodes();
    if (exit_nodes.size() == 1) {
      // If there is only one exit node, no need to unify
      continue;
    }
    // Create a new block as the unified exit block
    auto unified_exit_block = new mlir::Block();
    // Then emplace back this block to the function
    func_op->AddBlock(unified_exit_block);
    // For each exit node, we will change the return operation
    // to branch to the unified exit block

    // Collect the operands of the return operation,
    // which will be used as the operands of the branch operation to the unified exit block
    std::vector<mlir::Value *> return_operands;
    std::vector<mlir::Block *> branch_targets;
    type::Type *return_type = func_op->GetReturnType();
    for (auto exit_node : exit_nodes) {
      auto mlir_exit_node = dynamic_cast<analysis::MLIRCFGNode *>(exit_node);
      if (mlir_exit_node == nullptr) {
        throw std::logic_error("Exit node is not of type MLIRCFGNode\n");
      }
      auto exit_block = mlir_exit_node->GetBlock();
      if (exit_block == nullptr) {
        throw std::logic_error("Exit block in MLIRCFGNode is null\n");
      }
      // Get the terminator operation of the exit block, which should be a return operation
      // After simplifying the CFG,
      // we have guaranteed that there is only one terminator operation
      // in the block
      auto terminator_op = exit_block->operations_.back();

      if (auto ret_op = mlir::DynamicOrNull<mlir::func::ReturnOp>(terminator_op)) {
        if (ret_op->GetReturnValue() != nullptr) {
          // Has return value, need to record the value to generate a phi node in the unified exit block
          return_operands.emplace_back(ret_op->GetReturnValue());
          branch_targets.emplace_back(exit_block);
        }
        // Create a branch operation to the unified exit block
        auto br_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::BrOp>(unified_exit_block);
        // Replace the return operation with the branch operation
        exit_block->operations_.back() = br_op;
      } else {
        throw std::logic_error("Terminator operation in exit block is not a ReturnOp\n");
      }
    }

    // Then create a phi node in the unified exit block to merge the return values from different exit nodes
    if (!return_operands.empty()) {
      auto result = GET_MLIR_CONTEXT()->CreateValue();
      result->SetType(return_type);
      auto phi_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::PhiOp>(result, return_operands, branch_targets);
      unified_exit_block->operations_.emplace_back(phi_op);
      // Finally, add a return operation in the unified exit block to return the value from the phi node
      auto ret_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::func::ReturnOp>(GET_MLIR_CONTEXT(), return_type, result);
      unified_exit_block->operations_.emplace_back(ret_op);
    } else {
      // If there is no return value, we can directly add a return operation in the unified exit block
      auto ret_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::func::ReturnOp>(GET_MLIR_CONTEXT(), return_type);
      unified_exit_block->operations_.emplace_back(ret_op);
    }
  }
}

}  // namespace middleend::pass::transform
