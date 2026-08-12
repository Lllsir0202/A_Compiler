#include "middleend/pass/analysis/cfg/llvmcfg.h"
#include <queue>
#include "context/compiler_context.h"
#include "llvm/inst/branch/branch.h"
#include "llvm/inst/opkind.h"
#define GET_LLVM_CONTEXT() compiler_context_->GetIRContext()->GetLLVMIRContext()
namespace middleend::pass::analysis {
auto LLVMIRCFGBuilder::BuildCFG() -> void {
  Reset();
  auto llvm_module = GET_LLVM_CONTEXT()->GetModule();
  // Since the LLVMIR function is a struct kind of object,
  // it will never involve the library function, so we don't need to check.
  for (auto *llvm_function : llvm_module->GetFunctions()) {
    BuildFunctionCFG(llvm_function);
  }
}

// Build a CFG for a single LLVMIR function
auto LLVMIRCFGBuilder::BuildFunctionCFG(middleend::llvm::utils::Function *function) -> void {
  auto function_cfg = new LLVMIRFunctionCFG(function);
  function_cfgs_.emplace_back(function_cfg);
  auto entry_block = function->GetEntryBlock();
  auto entry_node = new LLVMIRCFGNode(entry_block);
  function_cfg->AddNode(entry_node);
  function_cfg->SetEntryNode(entry_node);
  // Use a worklist to build the CFG, starting from the entry block
  std::queue<LLVMIRCFGNode *> worklist;
  worklist.push(entry_node);
  std::unordered_set<middleend::llvm::utils::BasicBlock *> visited;
  visited.insert(entry_block);
  while (!worklist.empty()) {
    auto current_node = worklist.front();
    worklist.pop();
    // it represents a dfs naturally.
    function_cfg->PushDFSOrder(current_node);
    auto current_block = current_node->GetBlock();
    // Get the terminator instruction of the current block, and find its successor blocks
    auto terminator_inst = current_block->GetInstructions().back();
    if (terminator_inst->VerifyOpKind(llvm::inst::OpKind::Br)) {
      auto br_inst = static_cast<llvm::inst::BrInst *>(terminator_inst);
      auto target_block_id = br_inst->GetTargetBlockID();
      auto target_block = function->GetBasicBlockByID(target_block_id);
      if (target_block == nullptr) {
        compiler_context_->ReportError("Target block not found for block id: " + std::to_string(target_block_id));
        return;
      }
      // Get the target block.
      if (visited.count(target_block) == 0) {
        auto target_node = new LLVMIRCFGNode(target_block);
        function_cfg->AddNode(target_node);
        worklist.push(target_node);
        visited.insert(target_block);
        function_cfg->MapBlockToNode(target_block, target_node);
      }
      auto target_node = function_cfg->GetNodeByBlock(target_block);
      current_node->AddSuccessor(target_node);
      target_node->AddPredecessor(current_node);
      continue;
    }
    if (terminator_inst->VerifyOpKind(llvm::inst::OpKind::CondBr)) {
      auto cond_br_inst = static_cast<llvm::inst::BrCondInst *>(terminator_inst);
      auto true_block_id = cond_br_inst->GetTrueTargetBlockID();
      auto false_block_id = cond_br_inst->GetFalseTargetBlockID();
      auto true_block = function->GetBasicBlockByID(true_block_id);
      auto false_block = function->GetBasicBlockByID(false_block_id);
      if (true_block == nullptr || false_block == nullptr) {
        compiler_context_->ReportError("Target block not found for block id: " + std::to_string(true_block_id) +
                                       " or " + std::to_string(false_block_id));
        return;
      }
      // Get the target blocks.
      if (visited.count(true_block) == 0) {
        auto true_node = new LLVMIRCFGNode(true_block);
        function_cfg->AddNode(true_node);
        worklist.push(true_node);
        visited.insert(true_block);
        function_cfg->MapBlockToNode(true_block, true_node);
      }
      if (visited.count(false_block) == 0) {
        auto false_node = new LLVMIRCFGNode(false_block);
        function_cfg->AddNode(false_node);
        worklist.push(false_node);
        visited.insert(false_block);
        function_cfg->MapBlockToNode(false_block, false_node);
      }
      auto true_node = function_cfg->GetNodeByBlock(true_block);
      auto false_node = function_cfg->GetNodeByBlock(false_block);
      current_node->AddSuccessor(true_node);
      current_node->AddSuccessor(false_node);
      true_node->AddPredecessor(current_node);
      false_node->AddPredecessor(current_node);
      continue;
    }
    if (terminator_inst->VerifyOpKind(llvm::inst::OpKind::Ret)) {
      function_cfg->AddExitNode(current_node);
      continue;
    }
  }
}

}  // namespace middleend::pass::analysis