#include "middleend/pass/transform/loopsimplify/loopsimplify.h"
#include <algorithm>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/base/branch.h"
#include "middleend/mlir/dialect/base/funcdef.h"
#include "middleend/mlir/dialect/base/phi.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/block.h"
#include "middleend/mlir/utils/value/value.h"
#include "middleend/pass/analysis/cfg/cfg.h"
#include "middleend/pass/analysis/cfg/mlircfg.h"
#include "middleend/pass/analysis/loopinfo/loopinfo.h"

#define GET_MLIR_CONTEXT() compiler_context_->GetIRContext()->GetMLIRContext()
#define GET_PASS_CONTEXT() compiler_context_->GetPassContext()

namespace middleend::pass::transform {
namespace {
int preheader_counter = 0;

auto Terminator(mlir::Block *block) -> mlir::Operation * {
  return block->operations_.empty() ? nullptr : block->operations_.back();
}

auto Successors(mlir::Block *block) -> std::vector<mlir::Block *> {
  auto *terminator = Terminator(block);
  if (auto *br = mlir::DynamicOrNull<mlir::base::BrOp>(terminator)) {
    return {br->GetTarget()};
  }
  if (auto *cond_br = mlir::DynamicOrNull<mlir::base::CondBrOp>(terminator)) {
    return {cond_br->GetTrueTarget(), cond_br->GetFalseTarget()};
  }
  return {};
}

auto TargetsHeader(mlir::Block *block, mlir::Block *header) -> bool {
  auto *terminator = Terminator(block);
  if (auto *br = mlir::DynamicOrNull<mlir::base::BrOp>(terminator)) {
    return br->GetTarget() == header;
  }
  if (auto *cond_br = mlir::DynamicOrNull<mlir::base::CondBrOp>(terminator)) {
    return cond_br->GetTrueTarget() == header || cond_br->GetFalseTarget() == header;
  }
  return false;
}
}  // namespace

auto LoopSimplifyPass::Run() -> void {
  PreRun();
  auto *cfg_pass = GET_PASS_CONTEXT()->GetSpecPass<analysis::CFGPass>("CFGPass");
  auto *loop_info = GET_PASS_CONTEXT()->GetSpecPass<analysis::LoopInfoPass>("LoopInfoPass");
  if (cfg_pass == nullptr || loop_info == nullptr) {
    throw std::logic_error("LoopSimplifyPass requires CFGPass and LoopInfoPass");
  }

  bool changed = false;
  for (auto *function_cfg : cfg_pass->Get()) {
    auto *mlir_function_cfg = dynamic_cast<analysis::MLIRFunctionCFG *>(function_cfg);
    if (mlir_function_cfg == nullptr) {
      throw std::logic_error("LoopSimplifyPass only supports MLIRFunctionCFG");
    }
    auto *func_op = mlir_function_cfg->GetFuncOp();
    const auto &loops = loop_info->GetLoops(function_cfg);
    for (auto *loop : loops) {
      changed = EnsurePreheader(func_op, loop) || changed;
    }
    // LCSSA needs to know which block each operation lives in; rebuild after the preheader
    // insertions above so the map covers any block they added.
    op_block_.clear();
    for (auto *block : func_op->GetBlocks()) {
      for (auto *operation : block->operations_) {
        op_block_[operation] = block;
      }
    }
    // Innermost-first, so an inner loop's live-outs are already routed through its own exit phi
    // before the enclosing loop looks at them.
    for (auto it = loops.rbegin(); it != loops.rend(); ++it) {
      changed = FormLCSSA(func_op, *it) || changed;
    }
  }

  if (changed) {
    GET_PASS_CONTEXT()->InvalidateAnalysis();
  }
}

auto LoopSimplifyPass::EnsurePreheader(mlir::base::FuncDefOp *func_op, analysis::Loop *loop) -> bool {
  auto outside = OutsidePredecessors(func_op, loop);
  if (HasDedicatedPreheader(outside, loop->GetHeader())) {
    return false;
  }
  InsertPreheader(func_op, loop, outside);
  return true;
}

auto LoopSimplifyPass::OutsidePredecessors(mlir::base::FuncDefOp *func_op, analysis::Loop *loop)
    -> std::vector<mlir::Block *> {
  // Recompute from the live terminators so the result is correct even after an earlier insertion
  // in the same function mutated the control flow.
  std::vector<mlir::Block *> outside;
  for (auto *block : func_op->GetBlocks()) {
    if (loop->Contains(block)) {
      continue;
    }
    if (TargetsHeader(block, loop->GetHeader())) {
      outside.emplace_back(block);
    }
  }
  return outside;
}

auto LoopSimplifyPass::HasDedicatedPreheader(const std::vector<mlir::Block *> &outside, mlir::Block *header) -> bool {
  if (outside.size() != 1) {
    return false;
  }
  // The single outside predecessor must reach the header unconditionally (header as sole successor).
  auto *br = mlir::DynamicOrNull<mlir::base::BrOp>(Terminator(outside.front()));
  return br != nullptr && br->GetTarget() == header;
}

auto LoopSimplifyPass::InsertPreheader(mlir::base::FuncDefOp *func_op, analysis::Loop *loop,
                                       const std::vector<mlir::Block *> &outside) -> void {
  auto *header = loop->GetHeader();
  auto *preheader = new mlir::Block();
  preheader->block_name_ = "preheader";
  preheader->block_id_ = 900000 + preheader_counter++;

  // Redirect each header phi's incomings that come from outside the loop so they arrive from the
  // preheader instead. Several outside incomings are merged into a phi in the preheader.
  for (auto *operation : header->operations_) {
    auto *phi = mlir::DynamicOrNull<mlir::base::PhiOp>(operation);
    if (phi == nullptr) {
      continue;
    }
    auto values = phi->GetIncomingValue();
    auto blocks = phi->GetIncomingBlock();
    std::vector<mlir::Value *> outside_values;
    std::vector<mlir::Block *> outside_blocks;
    std::vector<mlir::Value *> inside_values;
    std::vector<mlir::Block *> inside_blocks;
    for (size_t i = 0; i < values.size(); ++i) {
      bool is_outside = false;
      for (auto *outside_block : outside) {
        if (blocks[i] == outside_block) {
          is_outside = true;
          break;
        }
      }
      if (is_outside) {
        outside_values.emplace_back(values[i]);
        outside_blocks.emplace_back(blocks[i]);
      } else {
        inside_values.emplace_back(values[i]);
        inside_blocks.emplace_back(blocks[i]);
      }
    }
    if (outside_values.empty()) {
      continue;
    }

    mlir::Value *incoming_from_preheader = nullptr;
    if (outside_values.size() == 1) {
      incoming_from_preheader = outside_values.front();
    } else {
      auto *merged = GET_MLIR_CONTEXT()->CreateValue();
      merged->SetType(phi->GetRes()->GetType());
      auto *merged_phi = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::PhiOp>(merged, outside_values, outside_blocks);
      for (size_t i = 0; i < outside_values.size(); ++i) {
        outside_values[i]->AddUse(merged_phi, i);
      }
      preheader->operations_.emplace_back(merged_phi);
      incoming_from_preheader = merged;
    }

    // Rebuild the header phi: keep the loop incomings and add one incoming from the preheader.
    for (auto *value : values) {
      value->RemoveUse(phi);
    }
    inside_values.emplace_back(incoming_from_preheader);
    inside_blocks.emplace_back(preheader);
    phi->SetIncomings(inside_values, inside_blocks);
    for (size_t i = 0; i < inside_values.size(); ++i) {
      inside_values[i]->AddUse(phi, i);
    }
  }

  auto *branch = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::BrOp>(header);
  preheader->operations_.emplace_back(branch);

  for (auto *block : outside) {
    RedirectTerminator(block, header, preheader);
  }

  // Place the preheader just before the header in the block list for readable output.
  auto blocks = func_op->GetBlocks();
  std::vector<mlir::Block *> reordered;
  reordered.reserve(blocks.size() + 1);
  for (auto *block : blocks) {
    if (block == header) {
      reordered.emplace_back(preheader);
    }
    reordered.emplace_back(block);
  }
  func_op->SetBlocks(std::move(reordered));
}

auto LoopSimplifyPass::FormLCSSA(mlir::base::FuncDefOp *func_op, analysis::Loop *loop) -> bool {
  // Exit block and the in-loop edges reaching it, read from the live terminators.
  mlir::Block *exit = nullptr;
  std::vector<mlir::Block *> exiting;
  for (auto *block : func_op->GetBlocks()) {
    if (!loop->Contains(block)) {
      continue;
    }
    for (auto *successor : Successors(block)) {
      if (successor == nullptr || loop->Contains(successor)) {
        continue;
      }
      if (exit != nullptr && exit != successor) {
        return false;  // several distinct exits: a use would need the right one, needs dominance
      }
      exit = successor;
      if (std::find(exiting.begin(), exiting.end(), block) == exiting.end()) {
        exiting.emplace_back(block);
      }
    }
  }
  if (exit == nullptr || exiting.empty()) {
    return false;
  }

  // Values defined in the loop, in block/operation order so the result is deterministic.
  std::vector<mlir::Value *> defined;
  for (auto *block : func_op->GetBlocks()) {
    if (!loop->Contains(block)) {
      continue;
    }
    for (auto *operation : block->operations_) {
      for (auto *result : operation->results_) {
        if (result != nullptr) {
          defined.emplace_back(result);
        }
      }
    }
  }

  // A use is outside the loop unless it sits in a loop block; a phi's use lives on its incoming
  // edge, and an exit phi fed from an in-loop edge is already LCSSA-legal, so it stays put.
  auto use_block = [&](mlir::Use *use) -> mlir::Block * {
    if (auto *phi = mlir::DynamicOrNull<mlir::base::PhiOp>(use->operation_)) {
      return phi->GetIncomingBlock()[use->operand_index_];
    }
    auto iter = op_block_.find(use->operation_);
    return iter == op_block_.end() ? nullptr : iter->second;
  };

  bool changed = false;
  for (auto *value : defined) {
    std::vector<mlir::Use *> outside;
    for (auto *use : value->GetUses()) {
      auto *block = use_block(use);
      if (block == nullptr || loop->Contains(block)) {
        continue;
      }
      outside.emplace_back(use);
    }
    if (outside.empty()) {
      continue;
    }

    // One incoming per in-loop edge into the exit; `value` dominates each of them because it
    // dominates its out-of-loop uses, which are only reachable through those edges.
    auto *result = GET_MLIR_CONTEXT()->CreateValue();
    result->SetType(value->GetType());
    std::vector<mlir::Value *> values(exiting.size(), value);
    auto *phi = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::PhiOp>(result, values, exiting);
    exit->operations_.insert(exit->operations_.begin(), phi);
    op_block_[phi] = exit;

    // Re-home the outside uses onto the new phi, then register the phi's own operands.
    std::vector<mlir::Use *> stay;
    std::unordered_set<mlir::Use *> moved(outside.begin(), outside.end());
    for (auto *use : value->GetUses()) {
      if (moved.count(use) == 0) {
        stay.emplace_back(use);
        continue;
      }
      use->operation_->operands_[use->operand_index_] = result;
      result->GetUses().emplace_back(use);
    }
    value->GetUses() = stay;
    for (size_t i = 0; i < values.size(); ++i) {
      value->AddUse(phi, i);
    }
    changed = true;
  }
  return changed;
}

auto LoopSimplifyPass::RedirectTerminator(mlir::Block *block, mlir::Block *from, mlir::Block *to) -> void {
  auto *terminator = Terminator(block);
  if (auto *br = mlir::DynamicOrNull<mlir::base::BrOp>(terminator)) {
    if (br->GetTarget() == from) {
      br->SetTarget(to);
    }
    return;
  }
  if (auto *cond_br = mlir::DynamicOrNull<mlir::base::CondBrOp>(terminator)) {
    if (cond_br->GetTrueTarget() == from) {
      cond_br->SetTrueTarget(to);
    }
    if (cond_br->GetFalseTarget() == from) {
      cond_br->SetFalseTarget(to);
    }
  }
}

}  // namespace middleend::pass::transform
