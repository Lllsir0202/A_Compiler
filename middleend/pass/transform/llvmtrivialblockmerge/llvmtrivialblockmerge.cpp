#include "middleend/pass/transform/llvmtrivialblockmerge/llvmtrivialblockmerge.h"
#include <deque>
#include <vector>
#include "context/compiler_context.h"
#include "middleend/llvm/inst/branch/branch.h"
#include "middleend/llvm/inst/branch/phi.h"
#include "middleend/llvm/utils/struct/block.h"
#include "middleend/llvm/utils/struct/function.h"
#include "middleend/llvm/utils/struct/module.h"

#define GET_LLVM_CONTEXT() compiler_context_->GetIRContext()->GetLLVMIRContext()
#define GET_PASS_CONTEXT() compiler_context_->GetPassContext()

namespace middleend::pass::transform {
auto LLVMIRTrivialBlockMergePass::Run() -> void {
  PreRun();
  auto *module = GET_LLVM_CONTEXT()->GetModule();
  if (module == nullptr) {
    throw std::logic_error("LLVMIRTrivialBlockMergePass requires LLVM IR module");
  }

  bool changed = false;
  for (auto *function : module->GetFunctions()) {
    changed = MergeInFunction(function) || changed;
  }
  if (changed) {
    GET_PASS_CONTEXT()->InvalidateAnalysis();
  }
}

auto LLVMIRTrivialBlockMergePass::MergeInFunction(llvm::utils::Function *function) -> bool {
  bool changed = false;
  bool local_changed = true;
  while (local_changed) {
    local_changed = false;
    auto predecessors = BuildPredecessors(function);
    if (TryMergeEntryBlock(function, predecessors)) {
      local_changed = true;
      changed = true;
      continue;
    }

    for (auto *block : function->GetBasicBlocks()) {
      if (!IsMergeableBlock(function, block) || !CanMergeBlock(function, block, predecessors)) {
        continue;
      }
      auto instructions = block->GetInstructions();
      auto *br_inst = dynamic_cast<llvm::inst::BrInst *>(instructions.front());
      auto *target = GetBlockByID(function, br_inst->GetTargetBlockID());
      auto predecessor_iter = predecessors.find(block);
      if (target == nullptr || target == block || predecessor_iter == predecessors.end()) {
        continue;
      }

      bool rewrote_pred = false;
      for (auto *predecessor : predecessor_iter->second) {
        if (predecessor == block) {
          continue;
        }
        auto predecessor_instructions = predecessor->GetInstructions();
        if (predecessor_instructions.empty()) {
          continue;
        }
        if (RewriteBranchTarget(predecessor_instructions.back(), block, target)) {
          RewritePhiIncoming(target, block, predecessor);
          rewrote_pred = true;
        }
      }
      if (rewrote_pred) {
        local_changed = true;
        changed = true;
        break;
      }
    }
  }
  if (changed) {
    RemoveMergedBlocks(function);
  }
  return changed;
}

auto LLVMIRTrivialBlockMergePass::BuildPredecessors(llvm::utils::Function *function)
    -> std::unordered_map<llvm::utils::BasicBlock *, std::unordered_set<llvm::utils::BasicBlock *>> {
  std::unordered_map<llvm::utils::BasicBlock *, std::unordered_set<llvm::utils::BasicBlock *>> predecessors;
  for (auto *block : function->GetBasicBlocks()) {
    auto instructions = block->GetInstructions();
    if (instructions.empty()) {
      continue;
    }
    auto *terminator = instructions.back();
    if (auto *br_inst = dynamic_cast<llvm::inst::BrInst *>(terminator)) {
      auto *target = GetBlockByID(function, br_inst->GetTargetBlockID());
      if (target != nullptr) {
        predecessors[target].insert(block);
      }
      continue;
    }
    if (auto *br_cond_inst = dynamic_cast<llvm::inst::BrCondInst *>(terminator)) {
      auto *true_target = GetBlockByID(function, br_cond_inst->GetTrueTargetBlockID());
      auto *false_target = GetBlockByID(function, br_cond_inst->GetFalseTargetBlockID());
      if (true_target != nullptr) {
        predecessors[true_target].insert(block);
      }
      if (false_target != nullptr) {
        predecessors[false_target].insert(block);
      }
    }
  }
  return predecessors;
}

auto LLVMIRTrivialBlockMergePass::TryMergeEntryBlock(
    llvm::utils::Function *function,
    const std::unordered_map<llvm::utils::BasicBlock *, std::unordered_set<llvm::utils::BasicBlock *>> &predecessors)
    -> bool {
  auto *entry_block = function->GetEntryBlock();
  auto entry_instructions = entry_block->GetInstructions();
  if (entry_instructions.size() != 1) {
    return false;
  }
  auto *br_inst = dynamic_cast<llvm::inst::BrInst *>(entry_instructions.front());
  if (br_inst == nullptr) {
    return false;
  }
  auto *target = GetBlockByID(function, br_inst->GetTargetBlockID());
  if (target == nullptr || target == entry_block || HasPhi(target)) {
    return false;
  }
  auto target_predecessor_iter = predecessors.find(target);
  if (target_predecessor_iter == predecessors.end() || target_predecessor_iter->second.size() != 1 ||
      target_predecessor_iter->second.count(entry_block) == 0) {
    return false;
  }

  auto target_instructions = target->GetInstructions();
  for (auto *instruction : target_instructions) {
    instruction->SetParentBlock(entry_block);
  }
  entry_block->SetInstructions(target_instructions);
  RewriteSuccessorPhiIncoming(function, entry_block, target, entry_block);
  RemoveBlock(function, target);
  return true;
}

auto LLVMIRTrivialBlockMergePass::IsMergeableBlock(llvm::utils::Function *function,
                                                   llvm::utils::BasicBlock *block) -> bool {
  if (block == nullptr || block == function->GetEntryBlock()) {
    return false;
  }
  auto instructions = block->GetInstructions();
  if (instructions.size() != 1) {
    return false;
  }
  auto *br_inst = dynamic_cast<llvm::inst::BrInst *>(instructions.front());
  return br_inst != nullptr && br_inst->GetTargetBlockID() != block->GetID();
}

auto LLVMIRTrivialBlockMergePass::CanMergeBlock(
    llvm::utils::Function *function, llvm::utils::BasicBlock *block,
    const std::unordered_map<llvm::utils::BasicBlock *, std::unordered_set<llvm::utils::BasicBlock *>> &predecessors)
    -> bool {
  auto instructions = block->GetInstructions();
  auto *br_inst = dynamic_cast<llvm::inst::BrInst *>(instructions.front());
  auto *target = GetBlockByID(function, br_inst->GetTargetBlockID());
  if (target == nullptr || !HasPhi(target)) {
    return true;
  }

  auto predecessor_iter = predecessors.find(block);
  if (predecessor_iter == predecessors.end() || predecessor_iter->second.size() != 1) {
    return false;
  }
  auto *predecessor = *predecessor_iter->second.begin();
  auto target_predecessor_iter = predecessors.find(target);
  if (target_predecessor_iter != predecessors.end() && target_predecessor_iter->second.count(predecessor) > 0) {
    return false;
  }
  return !HasPhiIncomingFrom(target, predecessor);
}

auto LLVMIRTrivialBlockMergePass::HasPhi(llvm::utils::BasicBlock *block) -> bool {
  auto instructions = block->GetInstructions();
  if (instructions.empty()) {
    return false;
  }
  return dynamic_cast<llvm::inst::PhiInst *>(instructions.front()) != nullptr;
}

auto LLVMIRTrivialBlockMergePass::HasPhiIncomingFrom(llvm::utils::BasicBlock *block,
                                                     llvm::utils::BasicBlock *incoming_block) -> bool {
  for (auto *instruction : block->GetInstructions()) {
    auto *phi_inst = dynamic_cast<llvm::inst::PhiInst *>(instruction);
    if (phi_inst == nullptr) {
      break;
    }
    for (auto *incoming : phi_inst->GetIncomingBlocks()) {
      if (incoming->GetID() == incoming_block->GetID()) {
        return true;
      }
    }
  }
  return false;
}

auto LLVMIRTrivialBlockMergePass::RewriteBranchTarget(llvm::inst::Instruction *instruction,
                                                      llvm::utils::BasicBlock *old_target,
                                                      llvm::utils::BasicBlock *new_target) -> bool {
  auto *new_label = GET_LLVM_CONTEXT()->GetResourceContext()->GetLabelOperand(new_target->GetID());
  if (auto *br_inst = dynamic_cast<llvm::inst::BrInst *>(instruction)) {
    if (br_inst->GetTargetBlockID() != old_target->GetID()) {
      return false;
    }
    br_inst->SetTarget(new_label);
    return true;
  }
  if (auto *br_cond_inst = dynamic_cast<llvm::inst::BrCondInst *>(instruction)) {
    bool changed = false;
    if (br_cond_inst->GetTrueTargetBlockID() == old_target->GetID()) {
      br_cond_inst->SetTrueTarget(new_label);
      changed = true;
    }
    if (br_cond_inst->GetFalseTargetBlockID() == old_target->GetID()) {
      br_cond_inst->SetFalseTarget(new_label);
      changed = true;
    }
    return changed;
  }
  return false;
}

auto LLVMIRTrivialBlockMergePass::RewritePhiIncoming(llvm::utils::BasicBlock *target,
                                                     llvm::utils::BasicBlock *old_block,
                                                     llvm::utils::BasicBlock *new_block) -> void {
  auto *new_label = GET_LLVM_CONTEXT()->GetResourceContext()->GetLabelOperand(new_block->GetID());
  for (auto *instruction : target->GetInstructions()) {
    auto *phi_inst = dynamic_cast<llvm::inst::PhiInst *>(instruction);
    if (phi_inst == nullptr) {
      break;
    }
    auto incoming_values = phi_inst->GetIncomingValues();
    auto incoming_blocks = phi_inst->GetIncomingBlocks();
    for (size_t i = 0; i < incoming_blocks.size(); ++i) {
      if (incoming_blocks[i]->GetID() == old_block->GetID()) {
        phi_inst->UpdateIncoming(i, incoming_values[i], new_label);
      }
    }
  }
}

auto LLVMIRTrivialBlockMergePass::RewriteSuccessorPhiIncoming(llvm::utils::Function *function,
                                                              llvm::utils::BasicBlock *block,
                                                              llvm::utils::BasicBlock *old_block,
                                                              llvm::utils::BasicBlock *new_block) -> void {
  auto instructions = block->GetInstructions();
  if (instructions.empty()) {
    return;
  }
  auto *terminator = instructions.back();
  if (auto *br_inst = dynamic_cast<llvm::inst::BrInst *>(terminator)) {
    auto *target = GetBlockByID(function, br_inst->GetTargetBlockID());
    if (target != nullptr) {
      RewritePhiIncoming(target, old_block, new_block);
    }
    return;
  }
  if (auto *br_cond_inst = dynamic_cast<llvm::inst::BrCondInst *>(terminator)) {
    auto *true_target = GetBlockByID(function, br_cond_inst->GetTrueTargetBlockID());
    auto *false_target = GetBlockByID(function, br_cond_inst->GetFalseTargetBlockID());
    if (true_target != nullptr) {
      RewritePhiIncoming(true_target, old_block, new_block);
    }
    if (false_target != nullptr && false_target != true_target) {
      RewritePhiIncoming(false_target, old_block, new_block);
    }
  }
}

auto LLVMIRTrivialBlockMergePass::RemoveBlock(llvm::utils::Function *function,
                                              llvm::utils::BasicBlock *block_to_remove) -> void {
  std::vector<llvm::utils::BasicBlock *> new_blocks;
  for (auto *block : function->GetBasicBlocks()) {
    if (block != block_to_remove) {
      new_blocks.emplace_back(block);
    }
  }
  function->SetBasicBlocks(std::move(new_blocks));
}

auto LLVMIRTrivialBlockMergePass::RemoveMergedBlocks(llvm::utils::Function *function) -> void {
  auto predecessors = BuildPredecessors(function);
  std::vector<llvm::utils::BasicBlock *> new_blocks;
  for (auto *block : function->GetBasicBlocks()) {
    if (IsMergeableBlock(function, block) && predecessors[block].empty()) {
      continue;
    }
    new_blocks.emplace_back(block);
  }
  function->SetBasicBlocks(std::move(new_blocks));
}

auto LLVMIRTrivialBlockMergePass::GetBlockByID(llvm::utils::Function *function,
                                               size_t block_id) -> llvm::utils::BasicBlock * {
  for (auto *block : function->GetBasicBlocks()) {
    if (block->GetID() == block_id) {
      return block;
    }
  }
  return nullptr;
}

}  // namespace middleend::pass::transform
