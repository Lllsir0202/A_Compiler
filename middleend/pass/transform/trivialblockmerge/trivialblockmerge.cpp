#include "middleend/pass/transform/trivialblockmerge/trivialblockmerge.h"
#include <vector>
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/base/branch.h"
#include "middleend/mlir/dialect/base/funcdef.h"
#include "middleend/mlir/dialect/base/phi.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/block.h"

#define GET_MLIR_CONTEXT() compiler_context_->GetIRContext()->GetMLIRContext()
#define GET_PASS_CONTEXT() compiler_context_->GetPassContext()

namespace middleend::pass::transform {
auto TrivialBlockMergePass::Run() -> void {
  PreRun();
  bool changed = false;
  auto *module_op = GET_MLIR_CONTEXT()->GetModuleOp();
  for (auto *operation : module_op->GetBlock()->operations_) {
    auto *func_op = mlir::DynamicOrNull<mlir::base::FuncDefOp>(operation);
    if (func_op == nullptr) {
      continue;
    }
    changed = MergeInFunction(func_op) || changed;
  }
  if (changed) {
    GET_PASS_CONTEXT()->InvalidateAnalysis();
  }
}

auto TrivialBlockMergePass::MergeInFunction(mlir::base::FuncDefOp *func_op) -> bool {
  bool changed = false;
  bool local_changed = true;
  while (local_changed) {
    local_changed = false;
    auto predecessors = BuildPredecessors(func_op);
    for (auto *block : func_op->GetBlocks()) {
      if (!IsMergeableBlock(func_op, block) || !CanMergeBlock(block, predecessors)) {
        continue;
      }
      auto *br_op = mlir::DynamicOrNull<mlir::base::BrOp>(block->operations_.front());
      auto *target = br_op->GetTarget();
      if (target == block) {
        continue;
      }
      auto iter = predecessors.find(block);
      if (iter == predecessors.end()) {
        continue;
      }
      bool rewrote_pred = false;
      for (auto *predecessor : iter->second) {
        if (predecessor == block) {
          continue;
        }
        if (RewriteBranchTarget(predecessor->operations_.back(), block, target)) {
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
    RemoveMergedBlocks(func_op);
  }
  return changed;
}

auto TrivialBlockMergePass::BuildPredecessors(mlir::base::FuncDefOp *func_op)
    -> std::unordered_map<mlir::Block *, std::unordered_set<mlir::Block *>> {
  std::unordered_map<mlir::Block *, std::unordered_set<mlir::Block *>> predecessors;
  for (auto *block : func_op->GetBlocks()) {
    if (block->operations_.empty()) {
      continue;
    }
    auto *terminator = block->operations_.back();
    if (auto *br_op = mlir::DynamicOrNull<mlir::base::BrOp>(terminator)) {
      predecessors[br_op->GetTarget()].insert(block);
      continue;
    }
    if (auto *cond_br_op = mlir::DynamicOrNull<mlir::base::CondBrOp>(terminator)) {
      predecessors[cond_br_op->GetTrueTarget()].insert(block);
      predecessors[cond_br_op->GetFalseTarget()].insert(block);
    }
  }
  return predecessors;
}

auto TrivialBlockMergePass::IsMergeableBlock(mlir::base::FuncDefOp *func_op, mlir::Block *block) -> bool {
  if (block == nullptr || block == func_op->GetEntryBlock()) {
    return false;
  }
  if (block->operations_.size() != 1) {
    return false;
  }
  auto *br_op = mlir::DynamicOrNull<mlir::base::BrOp>(block->operations_.front());
  if (br_op == nullptr || br_op->GetTarget() == block) {
    return false;
  }
  return true;
}

auto TrivialBlockMergePass::CanMergeBlock(
    mlir::Block *block, const std::unordered_map<mlir::Block *, std::unordered_set<mlir::Block *>> &predecessors)
    -> bool {
  auto *br_op = mlir::DynamicOrNull<mlir::base::BrOp>(block->operations_.front());
  auto *target = br_op->GetTarget();
  if (!HasPhi(target)) {
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

auto TrivialBlockMergePass::HasPhi(mlir::Block *block) -> bool {
  for (auto *operation : block->operations_) {
    if (mlir::DynamicOrNull<mlir::base::PhiOp>(operation) != nullptr) {
      return true;
    }
    break;
  }
  return false;
}

auto TrivialBlockMergePass::HasPhiIncomingFrom(mlir::Block *block, mlir::Block *incoming_block) -> bool {
  for (auto *operation : block->operations_) {
    auto *phi_op = mlir::DynamicOrNull<mlir::base::PhiOp>(operation);
    if (phi_op == nullptr) {
      break;
    }
    for (auto *block : phi_op->GetIncomingBlock()) {
      if (block == incoming_block) {
        return true;
      }
    }
  }
  return false;
}

auto TrivialBlockMergePass::RewriteBranchTarget(mlir::Operation *operation, mlir::Block *old_target,
                                                mlir::Block *new_target) -> bool {
  if (auto *br_op = mlir::DynamicOrNull<mlir::base::BrOp>(operation)) {
    if (br_op->GetTarget() != old_target) {
      return false;
    }
    br_op->SetTarget(new_target);
    return true;
  }
  if (auto *cond_br_op = mlir::DynamicOrNull<mlir::base::CondBrOp>(operation)) {
    bool changed = false;
    if (cond_br_op->GetTrueTarget() == old_target) {
      cond_br_op->SetTrueTarget(new_target);
      changed = true;
    }
    if (cond_br_op->GetFalseTarget() == old_target) {
      cond_br_op->SetFalseTarget(new_target);
      changed = true;
    }
    return changed;
  }
  return false;
}

auto TrivialBlockMergePass::RewritePhiIncoming(mlir::Block *target, mlir::Block *old_block, mlir::Block *new_block)
    -> void {
  for (auto *operation : target->operations_) {
    auto *phi_op = mlir::DynamicOrNull<mlir::base::PhiOp>(operation);
    if (phi_op == nullptr) {
      break;
    }
    auto incoming_values = phi_op->GetIncomingValue();
    auto incoming_blocks = phi_op->GetIncomingBlock();
    for (size_t i = 0; i < incoming_blocks.size(); ++i) {
      if (incoming_blocks[i] != old_block) {
        continue;
      }
      phi_op->UpdateIncoming(i, incoming_values[i], new_block);
    }
  }
}

auto TrivialBlockMergePass::RemoveMergedBlocks(mlir::base::FuncDefOp *func_op) -> void {
  auto predecessors = BuildPredecessors(func_op);
  std::vector<mlir::Block *> new_blocks;
  for (auto *block : func_op->GetBlocks()) {
    if (IsMergeableBlock(func_op, block) && predecessors[block].empty()) {
      continue;
    }
    new_blocks.emplace_back(block);
  }
  func_op->SetBlocks(std::move(new_blocks));
}
}  // namespace middleend::pass::transform
