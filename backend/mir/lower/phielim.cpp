#include "backend/mir/lower/phielim.h"
#include <algorithm>
#include <unordered_map>
#include <vector>
#include "backend/mir/inst/branch/branch.h"
#include "backend/mir/inst/pseudo/copy.h"
#include "backend/mir/inst/pseudo/phi.h"
#include "backend/mir/utils/operand/labeloperand.h"
#include "backend/mir/utils/struct/block.h"

namespace backend::lower {
static auto GetLabelId(utils::Operand *operand) -> size_t {
  return static_cast<utils::LabelOperand *>(operand)->GetId();
}

static auto GetSuccessorIds(inst::Instruction *terminal) -> std::vector<size_t> {
  std::vector<size_t> successors;
  if (auto *br = inst::DynamicOrNull<inst::BrInst>(terminal)) {
    successors.emplace_back(GetLabelId(br->GetOperands()[0]));
  } else if (auto *cond_br = inst::DynamicOrNull<inst::BrCondInst>(terminal)) {
    const auto &operands = cond_br->GetOperands();
    const auto true_id = GetLabelId(operands[1]);
    const auto false_id = GetLabelId(operands[2]);
    successors.emplace_back(true_id);
    if (false_id != true_id) {
      successors.emplace_back(false_id);
    }
  }
  return successors;
}

static auto RewriteBranchTarget(inst::Instruction *terminal, size_t old_target,
                                utils::LabelOperand *new_target) -> void {
  if (auto *br = inst::DynamicOrNull<inst::BrInst>(terminal)) {
    if (GetLabelId(br->GetOperands()[0]) == old_target) {
      br->SetOperand(0, new_target);
    }
    return;
  }

  if (auto *cond_br = inst::DynamicOrNull<inst::BrCondInst>(terminal)) {
    const auto &operands = cond_br->GetOperands();
    if (GetLabelId(operands[1]) == old_target) {
      cond_br->SetOperand(1, new_target);
    }
    if (GetLabelId(operands[2]) == old_target) {
      cond_br->SetOperand(2, new_target);
    }
  }
}

static auto RewritePhiIncoming(utils::Block *block, size_t old_block, utils::LabelOperand *new_block) -> void {
  for (auto *inst : block->GetInstructions()) {
    auto *phi = inst::DynamicOrNull<inst::PhiInst>(inst);
    if (phi == nullptr) {
      // Phi instructions are always at the beginning of the block
      break;
    }
    const auto &incoming_blocks = phi->GetIncomingBlocks();
    for (size_t i = 0; i < incoming_blocks.size(); ++i) {
      if (incoming_blocks[i]->GetId() == old_block) {
        phi->SetIncomingBlock(i, new_block);
      }
    }
  }
}

auto PhiElimination::EliminatePhi(utils::Function *function) -> void {
  // Recognize and split critical edges first
  SplitCriticalEdge(function);
  // Then lower phi instructions
  LowerPhi(function);
}

auto PhiElimination::SplitCriticalEdge(utils::Function *function) -> void {
  const auto &blocks = function->GetBlocks();
  std::unordered_map<size_t, utils::Block *> block_by_id;
  std::unordered_map<size_t, size_t> predecessor_count;
  block_by_id.reserve(blocks.size());
  predecessor_count.reserve(blocks.size() * 2);

  size_t next_block_id = 0;
  for (auto *block : blocks) {
    block_by_id[block->GetId()] = block;
    next_block_id = std::max(next_block_id, block->GetId() + 1);
    const auto &instructions = block->GetInstructions();
    if (!instructions.empty() && instructions.back()->IsBranch()) {
      for (auto successor_id : GetSuccessorIds(instructions.back())) {
        ++predecessor_count[successor_id];
      }
    }
  }

  std::vector<utils::Block *> new_blocks;
  new_blocks.reserve(blocks.size());
  for (auto *block : blocks) {
    new_blocks.emplace_back(block);
    const auto &instructions = block->GetInstructions();
    if (instructions.empty() || !instructions.back()->IsBranch()) {
      continue;
    }

    auto *terminal = instructions.back();
    const auto successor_ids = GetSuccessorIds(terminal);
    if (successor_ids.size() <= 1) {
      continue;
    }

    for (auto successor_id : successor_ids) {
      if (predecessor_count[successor_id] <= 1) {
        continue;
      }

      auto *successor = block_by_id[successor_id];
      auto *split_block = mir_context_->GetResourceContext()->CreateBlock(next_block_id++);
      auto *split_label = mir_context_->GetLabelOperand(split_block->GetId());
      auto *successor_label = mir_context_->GetLabelOperand(successor_id);
      auto *jump = mir_context_->GetResourceContext()->CreateInstruction<inst::BrInst>(mir_context_, successor_label);
      jump->SetParentBlock(split_block);
      split_block->AddInstBack(jump);

      RewriteBranchTarget(terminal, successor_id, split_label);
      RewritePhiIncoming(successor, block->GetId(), split_label);
      new_blocks.emplace_back(split_block);
    }
  }

  function->SetBlocks(std::move(new_blocks));
}

auto PhiElimination::LowerPhi(utils::Function *function) -> void {
  // For each phi instruction, we will replace it with copy instructions in the predecessor blocks
  for (auto *block : function->GetBlocks()) {
    std::deque<inst::Instruction *> new_instructions;
    for (auto *inst : block->GetInstructions()) {
      auto *phi = inst::DynamicOrNull<inst::PhiInst>(inst);
      if (phi == nullptr) {
        new_instructions.emplace_back(inst);
        continue;
      }

      const auto &incoming_values = phi->GetIncomingValues();
      const auto &incoming_blocks = phi->GetIncomingBlocks();
      for (size_t i = 0; i < incoming_values.size(); ++i) {
        auto *incoming_value = incoming_values[i];
        auto *incoming_block = incoming_blocks[i];
        auto *copy_inst = mir_context_->GetResourceContext()->CreateInstruction<inst::CopyInst>(
            mir_context_, incoming_value, phi->GetResult());
        copy_inst->SetParentBlock(incoming_block);
        incoming_block->AddInstBack(copy_inst);
      }
    }
    block->SetInstructions(new_instructions);
  }
}

}  // namespace backend::lower
