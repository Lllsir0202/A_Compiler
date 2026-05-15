#include "middleend/pass/transform/simplify/simplify.h"
#include <stack>
#include "context/compiler_context.h"
#include "middleend/llvm/inst/branch/branch.h"
#include "middleend/llvm/inst/branch/ret.h"
#include "middleend/llvm/utils/struct/block.h"
#include "middleend/llvm/utils/struct/function.h"
#include "middleend/mlir/dialect/base/branch.h"
#include "middleend/mlir/dialect/func/return.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/block.h"
#include "pass/pass.h"

#define GET_MLIR_CONTEXT() compiler_context_->GetIRContext()->GetMLIRContext()
#define GET_LLVM_CONTEXT() compiler_context_->GetIRContext()->GetLLVMIRContext()
namespace middleend::pass::transform {
auto SimplifyPass::Run() -> void {
  // Run simplify
  SimplifyCFG();
  compiler_context_->GetPassContext()->InvalidateAnalysis();
}

auto SimplifyPass::SimplifyCFG() -> void {
  if (pass_stage_ == PassStage::BASEIR) {
    SimplifyCFGBaseIR();
  } else if (pass_stage_ == PassStage::LLVMIR) {
    SimplifyCFGLLVMIR();
  } else {
    throw std::logic_error("SimplifyPass only supports BASEIR or LLVMIR stage");
  }
}

auto SimplifyPass::SimplifyCFGBaseIR() -> void {
  // get the baseir top module
  auto base_module = GET_MLIR_CONTEXT()->GetModuleOp();
  // From the entry block to run a DFS in every function
  auto entry = base_module->GetBlock();
  for (auto op : entry->operations_) {
    if (auto func_op = mlir::DynamicOrNull<mlir::base::FuncDefOp>(op)) {
      SimplifyCFGInFunction(func_op);
    }
  }
}

auto SimplifyPass::SimplifyCFGLLVMIR() -> void {
  // get the llvm ir module
  auto ir_module = GET_LLVM_CONTEXT()->GetModule();
  if (ir_module == nullptr) {
    throw std::logic_error("LLVM IR module is not set in LLVMIRContext");
  }
  // For each function in the module, simplify the cfg
  for (auto function : ir_module->GetFunctions()) {
    SimplifyCFGInLLVMFunction(function);
  }
}

// Helper function to simplify the cfg in a function
auto SimplifyPass::SimplifyCFGInFunction(mlir::base::FuncDefOp *func_op) -> void {
  // DFS to find reachable blocks
  std::unordered_set<mlir::Block *> visited;
  std::stack<mlir::Block *> stack;
  auto entry_block = func_op->GetEntryBlock();
  stack.push(entry_block);
  while (!stack.empty()) {
    auto block = stack.top();
    stack.pop();
    if (visited.count(block) > 0) {
      continue;
    }
    visited.insert(block);
    // Get the terminator operation of the block, and push the target blocks to the stack
    std::vector<mlir::Operation *> new_ops;
    for (auto &op : block->operations_) {
      new_ops.emplace_back(op);
      // Meet terminator, stop and push the target blocks to the stack
      // So that all ops after the terminator can be removed safely
      if (auto br_op = mlir::DynamicOrNull<mlir::base::BrOp>(op)) {
        auto target_block = br_op->GetTarget();
        stack.push(target_block);
        // Meet terminal, break
        break;
      }
      if (auto cond_br_op = mlir::DynamicOrNull<mlir::base::CondBrOp>(op)) {
        auto true_block = cond_br_op->GetTrueTarget();
        auto false_block = cond_br_op->GetFalseTarget();
        stack.push(true_block);
        stack.push(false_block);
        break;
      }
      if (auto ret_op = mlir::DynamicOrNull<mlir::func::ReturnOp>(op)) {
        break;
      }
    }
    block->operations_ = std::move(new_ops);
  }
  std::vector<mlir::Block *> new_blocks;
  for (auto block : func_op->GetBlocks()) {
    if (visited.count(block) > 0) {
      new_blocks.emplace_back(block);
    } else {
      delete block;
    }
  }
  func_op->SetBlocks(std::move(new_blocks));
}

auto SimplifyPass::SimplifyCFGInLLVMFunction(llvm::utils::Function *function) -> void {
  // DFS to find reachable blocks
  std::unordered_set<llvm::utils::BasicBlock *> visited;
  std::stack<llvm::utils::BasicBlock *> stack;
  auto entry_block = function->GetEntryBlock();
  stack.push(entry_block);
  while (!stack.empty()) {
    auto block = stack.top();
    stack.pop();
    if (visited.count(block) > 0) {
      continue;
    }
    visited.insert(block);
    // Get the terminator instruction of the block, and push the target blocks to the stack
    bool is_terminated = false;
    std::deque<llvm::inst::Instruction *> new_insts;
    for (auto &inst : block->GetInstructions()) {
      if (is_terminated) {
        delete inst;
        continue;
      }
      new_insts.push_back(inst);
      if (auto br_inst = dynamic_cast<llvm::inst::BrInst *>(inst)) {
        auto target_block_id = br_inst->GetTargetBlockID();
        auto target_block = GET_LLVM_CONTEXT()->GetResourceContext()->GetBlockByID(function, target_block_id);
        stack.push(target_block);
        is_terminated = true;
      }
      if (auto cond_br_inst = dynamic_cast<llvm::inst::BrCondInst *>(inst)) {
        auto true_block = cond_br_inst->GetTrueTargetBlockID();
        auto false_block = cond_br_inst->GetFalseTargetBlockID();
        auto llvm_true_block = GET_LLVM_CONTEXT()->GetResourceContext()->GetBlockByID(function, true_block);
        auto llvm_false_block = GET_LLVM_CONTEXT()->GetResourceContext()->GetBlockByID(function, false_block);
        stack.push(llvm_true_block);
        stack.push(llvm_false_block);
        is_terminated = true;
      }
      if (auto ret_inst = dynamic_cast<llvm::inst::RetInst *>(inst)) {
        is_terminated = true;
      }
    }
    block->SetInstructions(new_insts);
  }
  std::vector<llvm::utils::BasicBlock *> new_blocks;
  for (auto block : function->GetBasicBlocks()) {
    if (visited.count(block) > 0) {
      new_blocks.emplace_back(block);
    } else {
      delete block;
    }
  }
  function->SetBasicBlocks(std::move(new_blocks));
}

}  // namespace middleend::pass::transform