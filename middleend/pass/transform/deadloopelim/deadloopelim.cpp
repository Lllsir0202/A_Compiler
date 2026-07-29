#include "middleend/pass/transform/deadloopelim/deadloopelim.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <numeric>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>
#include "context/compiler_context.h"
#include "context/mlir_context.h"
#include "middleend/mlir/dialect/base/binaryop.h"
#include "middleend/mlir/dialect/base/branch.h"
#include "middleend/mlir/dialect/base/cast.h"
#include "middleend/mlir/dialect/base/funccall.h"
#include "middleend/mlir/dialect/base/funcdef.h"
#include "middleend/mlir/dialect/base/phi.h"
#include "middleend/mlir/dialect/base/unaryop.h"
#include "middleend/mlir/dialect/func/funccall.h"
#include "middleend/mlir/dialect/var/set.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/block.h"
#include "middleend/mlir/utils/value/value.h"
#include "middleend/pass/analysis/cfg/cfg.h"
#include "middleend/pass/analysis/cfg/mlircfg.h"
#include "middleend/pass/analysis/loopinfo/loopinfo.h"
#include "middleend/pass/analysis/scev/scev.h"
#include "middleend/pass/analysis/scev/scevpass.h"
#include "middleend/pass/analysis/sideeffect/sideeffect.h"
#include "utils/type/type.h"
#include "utils/type/type_factory.h"

#define GET_PASS_CONTEXT() compiler_context_->GetPassContext()
#define GET_MLIR_CONTEXT() compiler_context_->GetIRContext()->GetMLIRContext()

namespace middleend::pass::transform {
namespace {
// The out-of-loop / in-loop successor split for a terminator, used to find the exit edge.
auto Successors(mlir::Operation *terminator) -> std::vector<mlir::Block *> {
  std::vector<mlir::Block *> targets;
  if (auto *br = mlir::DynamicOrNull<mlir::base::BrOp>(terminator)) {
    targets.emplace_back(br->GetTarget());
  } else if (auto *cond_br = mlir::DynamicOrNull<mlir::base::CondBrOp>(terminator)) {
    targets.emplace_back(cond_br->GetTrueTarget());
    targets.emplace_back(cond_br->GetFalseTarget());
  }
  return targets;
}

// Whether the closed-form SCEV can be rebuilt from loop-invariant leaves alone (no recurrence).
auto CanExpand(const analysis::SCEV *scev) -> bool {
  switch (scev->GetKind()) {
    case analysis::SCEVKind::Constant:
      return static_cast<const analysis::SCEVConstant *>(scev)->IsInt();
    case analysis::SCEVKind::Unknown:
      return static_cast<const analysis::SCEVUnknown *>(scev)->GetValue() != nullptr;
    case analysis::SCEVKind::Add:
    case analysis::SCEVKind::Mul: {
      const auto &operands = static_cast<const analysis::SCEVCommutativeExpr *>(scev)->GetOperands();
      for (const auto *operand : operands) {
        if (!CanExpand(operand)) {
          return false;
        }
      }
      return true;
    }
    case analysis::SCEVKind::SMax: {
      const auto &operands = static_cast<const analysis::SCEVSMaxExpr *>(scev)->GetOperands();
      if (operands.size() != 2) {
        return false;
      }
      for (size_t index = 0; index < 2; ++index) {
        const auto *candidate = operands[index];
        if (candidate->GetKind() == analysis::SCEVKind::Constant &&
            static_cast<const analysis::SCEVConstant *>(candidate)->IsInt() &&
            static_cast<const analysis::SCEVConstant *>(candidate)->GetInt() == 0) {
          return CanExpand(operands[1 - index]);
        }
      }
      return false;
    }
    case analysis::SCEVKind::Div: {
      const auto *div = static_cast<const analysis::SCEVDivExpr *>(scev);
      return CanExpand(div->GetLHS()) && CanExpand(div->GetRHS());
    }
    case analysis::SCEVKind::RepeatedDiv: {
      const auto *repeat = static_cast<const analysis::SCEVRepeatedDivExpr *>(scev);
      return repeat->IsClosed() && CanExpand(repeat->GetStart()) && CanExpand(repeat->GetIteration());
    }
    case analysis::SCEVKind::Mod: {
      const auto *mod = static_cast<const analysis::SCEVModExpr *>(scev);
      return CanExpand(mod->GetLHS()) && CanExpand(mod->GetRHS());
    }
    case analysis::SCEVKind::ModAddRec: {
      const auto *mod_rec = static_cast<const analysis::SCEVModAddRecExpr *>(scev);
      return mod_rec->IsClosed() && CanExpand(mod_rec->GetStart()) && CanExpand(mod_rec->GetIteration());
    }
    default:
      return false;  // an AddRec / SMax / UMin cannot be materialized at a single point here
  }
}
}  // namespace

auto DeadLoopEliminatePass::Run() -> void {
  PreRun();
  if (std::getenv("DLE_OFF") != nullptr) {
    return;
  }
  auto *cfg_pass = GET_PASS_CONTEXT()->GetSpecPass<analysis::CFGPass>("CFGPass");
  auto *loop_info = GET_PASS_CONTEXT()->GetSpecPass<analysis::LoopInfoPass>("LoopInfoPass");
  scev_ = GET_PASS_CONTEXT()->GetSpecPass<analysis::SCEVPass>("SCEVPass");
  if (cfg_pass == nullptr || loop_info == nullptr || scev_ == nullptr) {
    throw std::logic_error("DeadLoopEliminatePass requires CFGPass, LoopInfoPass and SCEVPass");
  }

  bool changed = false;
  for (auto *function_cfg : cfg_pass->Get()) {
    auto *mlir_function_cfg = dynamic_cast<analysis::MLIRFunctionCFG *>(function_cfg);
    if (mlir_function_cfg == nullptr) {
      throw std::logic_error("DeadLoopEliminatePass only supports MLIRFunctionCFG");
    }
    auto *func_op = mlir_function_cfg->GetFuncOp();
    if (func_op == nullptr) {
      continue;
    }

    skipped_this_function_ = false;
    op_to_block_.clear();
    for (auto *cfg_node : mlir_function_cfg->GetNodes()) {
      auto *mlir_node = dynamic_cast<analysis::MLIRCFGNode *>(cfg_node);
      if (mlir_node == nullptr) {
        continue;
      }
      for (auto *operation : mlir_node->GetBlock()->operations_) {
        op_to_block_[operation] = mlir_node->GetBlock();
      }
    }

    // Independent loops can be deleted from the same cached loop forest: removing
    // one does not change another loop's header predecessors or exit edge. A
    // containing loop is skipped once any of its blocks has been removed; that
    // case needs freshly rebuilt analyses.
    for (auto *loop : loop_info->GetLoops(function_cfg)) {
      const auto live_block_list = func_op->GetBlocks();
      std::unordered_set<mlir::Block *> live_blocks(live_block_list.begin(), live_block_list.end());
      bool all_blocks_live = std::all_of(loop->GetBlocks().begin(), loop->GetBlocks().end(),
                                         [&](mlir::Block *block) { return live_blocks.count(block) > 0; });
      if (!all_blocks_live) {
        continue;
      }
      if (TryDeleteLoop(mlir_function_cfg, func_op, loop)) {
        changed = true;
      }
      if (skipped_this_function_) {
        // A skip rewrites a phi but leaves every block in place, so the cached SCEVs of the loops
        // still to be visited now describe stale start values. Deleting a loop cannot mislead the
        // others that way (its blocks are gone), but this can, so stop and let the analyses rebuild.
        break;
      }
    }
  }

  if (std::getenv("DLE_STATS") != nullptr) {
    std::fprintf(stderr, "[DLE] deleted loops: %d skipped-to-last: %d\n", deleted_count_, skipped_count_);
  }
  if (changed) {
    GET_PASS_CONTEXT()->InvalidateAnalysis();
  }
}

auto DeadLoopEliminatePass::FindPreheader(analysis::MLIRFunctionCFG *function_cfg,
                                          analysis::Loop *loop) -> mlir::Block * {
  auto *header_node = function_cfg->GetNodeByBlock(loop->GetHeader());
  if (header_node == nullptr) {
    return nullptr;
  }
  mlir::Block *preheader = nullptr;
  for (auto *predecessor : header_node->GetPredecessors()) {
    auto *mlir_pred = dynamic_cast<analysis::MLIRCFGNode *>(predecessor);
    if (mlir_pred == nullptr) {
      continue;
    }
    auto *pred_block = mlir_pred->GetBlock();
    if (loop->Contains(pred_block)) {
      continue;  // a latch
    }
    if (preheader != nullptr) {
      return nullptr;  // no dedicated preheader
    }
    preheader = pred_block;
  }
  return preheader;
}

auto DeadLoopEliminatePass::FindUniqueExit(analysis::Loop *loop, mlir::Block **exiting_out) -> mlir::Block * {
  mlir::Block *exit = nullptr;
  mlir::Block *exiting = nullptr;
  for (auto *block : loop->GetBlocks()) {
    if (block->operations_.empty()) {
      continue;
    }
    for (auto *target : Successors(block->operations_.back())) {
      if (target == nullptr || loop->Contains(target)) {
        continue;
      }
      if (exit != nullptr && (exit != target || exiting != block)) {
        return nullptr;  // more than one distinct exit edge
      }
      exit = target;
      exiting = block;
    }
  }
  *exiting_out = exiting;
  return exit;
}

auto DeadLoopEliminatePass::HasSideEffect(analysis::Loop *loop) -> bool {
  auto *se = GET_PASS_CONTEXT()->GetSpecPass<analysis::FunctionSideEffectPass>("FunctionSideEffectPass");
  for (auto *block : loop->GetBlocks()) {
    for (auto *operation : block->operations_) {
      if (mlir::DynamicOrNull<mlir::var::VarSetOp>(operation) != nullptr) return true;
      if (mlir::DynamicOrNull<mlir::base::FuncCallOp>(operation) != nullptr ||
          mlir::DynamicOrNull<mlir::func::FuncCallOp>(operation) != nullptr) {
        if (se == nullptr || !se->IsReadNoneAndIOClean(operation)) return true;
      }
    }
  }
  return false;
}
auto DeadLoopEliminatePass::TryDeleteLoop(analysis::MLIRFunctionCFG *function_cfg, mlir::base::FuncDefOp *func_op,
                                          analysis::Loop *loop) -> bool {
  auto *header = loop->GetHeader();
  auto *preheader = FindPreheader(function_cfg, loop);
  if (preheader == nullptr || preheader->operations_.empty()) {
    return false;
  }
  // The preheader must fall straight into the header so it can be retargeted at the exit.
  auto *preheader_br = mlir::DynamicOrNull<mlir::base::BrOp>(preheader->operations_.back());
  if (preheader_br == nullptr || preheader_br->GetTarget() != header) {
    return false;
  }
  const auto &latches = loop->GetLatches();
  if (latches.size() != 1) {
    return false;
  }
  // Canonical shape: the header's only predecessors are the preheader and the single latch.
  auto *header_node = function_cfg->GetNodeByBlock(header);
  if (header_node == nullptr || header_node->GetPredecessors().size() != 2) {
    return false;
  }
  for (auto *predecessor : header_node->GetPredecessors()) {
    auto *mlir_pred = dynamic_cast<analysis::MLIRCFGNode *>(predecessor);
    if (mlir_pred == nullptr || (mlir_pred->GetBlock() != preheader && mlir_pred->GetBlock() != latches[0])) {
      return false;
    }
  }

  // Single-exit: the one edge leaving the loop may start at the header (top-tested) or at the latch
  // after rotation (bottom-tested). Trip-count and exit-value reasoning below comes from SCEV, so it
  // does not depend on the concrete phi/CFG shape.
  mlir::Block *exiting = nullptr;
  mlir::Block *exit = FindUniqueExit(loop, &exiting);
  if (exit == nullptr || exiting == nullptr) {
    return false;
  }

  if (HasSideEffect(loop)) {
    return false;
  }

  // Resolve the loop's trip count. Accept both compile-time constants and symbolic
  // (loop-invariant) expressions; only an opaque Unknown sentinel blocks the transform.
  const analysis::SCEV *backedge = scev_->GetBackedgeTakenCount(loop);
  if (backedge->GetKind() == analysis::SCEVKind::Unknown) {
    // No trip count, so the loop cannot be deleted -- but if only its last iteration is observable it
    // can still be started there, which needs the exit test rather than a count.
    return TrySkipToLastIteration(function_cfg, loop, preheader, exiting);
  }
  const analysis::SCEV *raw_trip = scev_->GetAddExpr({backedge, scev_->GetConstant(1)});
  // Preserve SMax(0, count): it is what gives a top-tested loop zero
  // iterations when its initial condition is false.
  const analysis::SCEV *trip_scev = raw_trip;
  bool symbolic_trip = scev_->GetConstantTripCount(loop) < 0;
  if (symbolic_trip && !CanExpand(trip_scev)) {
    return false;
  }
  const analysis::SCEV *exit_iteration_scev =
      exiting == header ? trip_scev : scev_->GetMinusSCEV(trip_scev, scev_->GetConstant(1));
  if (!CanExpand(exit_iteration_scev)) {
    return false;
  }

  // Every value defined in the loop and live outside it must close to an expandable value. In LCSSA
  // form those live-outs normally appear as exit phis. For top-tested loops the value on the exit
  // edge is evaluated at `trip`; for rotated bottom-tested loops it is evaluated at `trip - 1`,
  // because the exiting latch runs after the last body execution.
  std::vector<std::pair<mlir::Value *, const analysis::SCEV *>> to_close;
  for (auto *block : loop->GetBlocks()) {
    for (auto *operation : block->operations_) {
      for (auto *result : operation->results_) {
        bool live_out = false;
        for (auto *use : result->GetUses()) {
          auto iter = op_to_block_.find(use->operation_);
          if (iter == op_to_block_.end() || !loop->Contains(iter->second)) {
            live_out = true;
            break;
          }
        }
        if (!live_out) {
          continue;
        }
        const analysis::SCEV *scev = scev_->GetSCEV(result);
        const analysis::SCEV *closed = nullptr;
        bool loop_recurrence = (scev->GetKind() == analysis::SCEVKind::AddRec &&
                                static_cast<const analysis::SCEVAddRecExpr *>(scev)->GetLoop() == loop) ||
                               (scev->GetKind() == analysis::SCEVKind::RepeatedDiv &&
                                !static_cast<const analysis::SCEVRepeatedDivExpr *>(scev)->IsClosed() &&
                                static_cast<const analysis::SCEVRepeatedDivExpr *>(scev)->GetLoop() == loop) ||
                               (scev->GetKind() == analysis::SCEVKind::ModAddRec &&
                                !static_cast<const analysis::SCEVModAddRecExpr *>(scev)->IsClosed() &&
                                static_cast<const analysis::SCEVModAddRecExpr *>(scev)->GetLoop() == loop);
        if (loop_recurrence) {
          // With a symbolic trip count, only affine AddRecs are supported: the closed form
          // start + step*iteration involves no division, so it is exact under modular arithmetic.
          // Higher-order chains (running sums) need binomial coefficients that divide by
          // factorials, which requires a no-wrap guarantee that we do not yet track.
          if (symbolic_trip && scev->GetKind() == analysis::SCEVKind::AddRec &&
              !static_cast<const analysis::SCEVAddRecExpr *>(scev)->IsAffine()) {
            return false;
          }
          closed = scev_->EvaluateAtIteration(scev, exit_iteration_scev);
        } else if (scev_->IsLoopInvariant(scev, loop)) {
          closed = scev;
        }
        if (closed == nullptr || !CanExpand(closed)) {
          // Cannot express this live-out in closed form, so the loop cannot be deleted outright. It
          // may still be reducible to its final iteration.
          return TrySkipToLastIteration(function_cfg, loop, preheader, exiting);
        }
        to_close.emplace_back(result, closed);
      }
    }
  }

  // ---- Everything is legal; commit the transform. ----------------------------------------------

  // 1. Materialize each closed form in the preheader (its leaves dominate the preheader) and rewire
  //    every use to it. Uses inside the loop are rewired too, but those blocks are about to vanish.
  for (const auto &entry : to_close) {
    mlir::Value *closed_value = Expand(entry.second, preheader);
    if (closed_value == nullptr) {
      throw std::logic_error("DeadLoopEliminatePass: verified closed form failed to expand");
    }
    entry.first->ReplaceAllUsesWith(closed_value);
  }

  // 2. The exit block's phis now take their loop-edge value straight from the preheader.
  for (auto *operation : exit->operations_) {
    auto *phi = mlir::DynamicOrNull<mlir::base::PhiOp>(operation);
    if (phi == nullptr) {
      continue;
    }
    const auto &incoming_values = phi->GetIncomingValue();
    const auto &incoming_blocks = phi->GetIncomingBlock();
    for (size_t index = 0; index < incoming_blocks.size(); ++index) {
      if (incoming_blocks[index] == exiting) {
        phi->UpdateIncoming(index, incoming_values[index], preheader);
      }
    }
  }

  // 3. Branch the preheader straight to the exit.
  preheader_br->SetTarget(exit);

  // 4. Detach every loop operation from the values it used, then drop the loop blocks. Clearing the
  //    use-lists keeps them consistent for the analyses rebuilt after this pass.
  const std::unordered_set<mlir::Block *> &loop_blocks = loop->GetBlockSet();  // for O(1) membership below
  for (auto *block : loop_blocks) {
    for (auto *operation : block->operations_) {
      for (auto *operand : operation->operands_) {
        operand->RemoveUse(operation);
      }
    }
  }
  std::vector<mlir::Block *> remaining;
  for (auto *block : func_op->GetBlocks()) {
    if (loop_blocks.count(block) == 0) {
      remaining.emplace_back(block);
    }
  }
  func_op->SetBlocks(std::move(remaining));

  ++deleted_count_;
  return true;
}

auto DeadLoopEliminatePass::IsDeadAcrossIterations(analysis::Loop *loop, mlir::base::PhiOp *phi) -> bool {
  // A header phi is dead across iterations when nothing inside the loop ever reads it. Its value is
  // then recomputed from scratch every iteration and only the final one can be observed. Reads from
  // outside the loop are fine -- those see the final value either way.
  //
  // The phi's own back-edge operand is not a read: that is the definition being carried around.
  mlir::Value *result = phi->GetRes();
  for (auto *use : result->GetUses()) {
    auto *user = use->operation_;
    if (user == phi) {
      continue;
    }
    auto iter = op_to_block_.find(user);
    if (iter == op_to_block_.end() || !loop->Contains(iter->second)) {
      continue;  // read after the loop
    }
    // A read inside the loop by another header phi of the same loop is still a genuine read, so no
    // exception is made for phis here.
    return false;
  }
  return true;
}

auto DeadLoopEliminatePass::TrySkipToLastIteration(analysis::MLIRFunctionCFG *function_cfg, analysis::Loop *loop,
                                                   mlir::Block *preheader, mlir::Block *exiting) -> bool {
  auto *header = loop->GetHeader();
  // Only a top-tested loop is handled: the exit test must run before the body, so that entering with
  // the final induction value executes exactly the last iteration.
  if (exiting != header) {
    return false;
  }

  // The exit test has to be `iv != bound` (which is what `while (n)` lowers to) with a unit step, so
  // the last iteration is the single value `bound - step`. A relational test would not pin down a
  // unique last value without a trip count, and a non-unit step could step over the bound.
  auto *cond_br = mlir::DynamicOrNull<mlir::base::CondBrOp>(header->operations_.back());
  if (cond_br == nullptr) {
    return false;
  }
  mlir::Value *condition = cond_br->GetCondition();
  if (condition == nullptr || condition->op_ == nullptr) {
    return false;
  }
  // `while (n)` is a truthiness cast, i.e. `n != 0`.
  auto *to_bool = mlir::DynamicOrNull<mlir::base::Int2BoolOp>(condition->op_);
  if (to_bool == nullptr || !loop->Contains(cond_br->GetTrueTarget()) ||
      loop->Contains(cond_br->GetFalseTarget())) {
    return false;
  }
  mlir::Value *tested = to_bool->GetOperand();

  // Split the header phis into the induction variable being tested and the accumulators. Every
  // accumulator must be dead across iterations; if any one is read inside the loop then earlier
  // iterations are observable and nothing may be skipped.
  mlir::base::PhiOp *induction = nullptr;
  int step = 0;
  bool saw_dead_accumulator = false;
  for (auto *operation : header->operations_) {
    auto *phi = mlir::DynamicOrNull<mlir::base::PhiOp>(operation);
    if (phi == nullptr) {
      continue;
    }
    if (phi->GetRes() == tested) {
      const analysis::SCEV *scev = scev_->GetSCEV(phi->GetRes());
      if (scev->GetKind() != analysis::SCEVKind::AddRec ||
          static_cast<const analysis::SCEVAddRecExpr *>(scev)->GetLoop() != loop ||
          !static_cast<const analysis::SCEVAddRecExpr *>(scev)->IsAffine()) {
        return false;
      }
      const analysis::SCEV *step_scev = static_cast<const analysis::SCEVAddRecExpr *>(scev)->GetStep();
      if (step_scev->GetKind() != analysis::SCEVKind::Constant ||
          !static_cast<const analysis::SCEVConstant *>(step_scev)->IsInt()) {
        return false;
      }
      step = static_cast<const analysis::SCEVConstant *>(step_scev)->GetInt();
      induction = phi;
      continue;
    }
    if (!IsDeadAcrossIterations(loop, phi)) {
      return false;
    }
    saw_dead_accumulator = true;
  }
  if (induction == nullptr || !saw_dead_accumulator || (step != 1 && step != -1)) {
    return false;
  }

  // ---- Legal; commit. -------------------------------------------------------------------------
  //
  // Enter the loop holding the value the induction variable had on its final iteration, so the body
  // runs exactly once more and then the test fails. The last iteration's computation and the loop's
  // own exit value are both unchanged.
  //
  // The guard matters: if the start value is on the far side of the bound, the original loop runs
  // until the type wraps rather than terminating. Selecting the skipped start only when the loop was
  // going to terminate keeps that pathological case bit-identical instead of silently making it halt.
  mlir::Value *entry_value = nullptr;
  size_t entry_index = 0;
  const auto &incoming_values = induction->GetIncomingValue();
  const auto &incoming_blocks = induction->GetIncomingBlock();
  for (size_t index = 0; index < incoming_blocks.size(); ++index) {
    if (!loop->Contains(incoming_blocks[index])) {
      entry_value = incoming_values[index];
      entry_index = index;
    }
  }
  if (entry_value == nullptr) {
    return false;
  }

  mlir::Value *last_iteration = MakeConstInt(-step, preheader);  // bound(0) - step
  mlir::Value *zero = MakeConstInt(0, preheader);
  // Counting down terminates from a start above the bound; counting up, from below it.
  mlir::Value *terminates = step < 0 ? MakeComparison<mlir::base::GreaterOp>(entry_value, zero, preheader)
                                     : MakeComparison<mlir::base::LessOp>(entry_value, zero, preheader);
  mlir::Value *selector = MakeBoolToInt(terminates, preheader);
  mlir::Value *keep = MakeBinary<mlir::base::SubOp>(MakeConstInt(1, preheader), selector, preheader);
  mlir::Value *start_value =
      MakeBinary<mlir::base::AddOp>(MakeBinary<mlir::base::MulOp>(selector, last_iteration, preheader),
                                    MakeBinary<mlir::base::MulOp>(keep, entry_value, preheader), preheader);

  // UpdateIncoming does not maintain use lists, so move the use across by hand.
  entry_value->RemoveUse(induction);
  induction->UpdateIncoming(entry_index, start_value, incoming_blocks[entry_index]);
  start_value->AddUse(induction, entry_index);

  ++skipped_count_;
  skipped_this_function_ = true;
  return true;
}

auto DeadLoopEliminatePass::InsertBeforeTerminator(mlir::Operation *operation, mlir::Block *block) -> void {
  block->operations_.insert(block->operations_.end() - 1, operation);
  op_to_block_[operation] = block;
}

auto DeadLoopEliminatePass::MakeConstInt(int value, mlir::Block *at) -> mlir::Value * {
  auto *result = GET_MLIR_CONTEXT()->CreateValue();
  result->SetType(type::TypeFactory::GetIntegerType());
  auto *op = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::ConstIntOp>(result, value);
  InsertBeforeTerminator(op, at);
  return result;
}

template <typename BinaryOp>
auto DeadLoopEliminatePass::MakeBinary(mlir::Value *lhs, mlir::Value *rhs, mlir::Block *at) -> mlir::Value * {
  auto *result = GET_MLIR_CONTEXT()->CreateValue();
  result->SetType(lhs->GetType());
  auto *op = GET_MLIR_CONTEXT()->CreateOperation<BinaryOp>(result, lhs, rhs);
  lhs->AddUse(op, 0);
  rhs->AddUse(op, 1);
  InsertBeforeTerminator(op, at);
  return result;
}

template <typename CompareOp>
auto DeadLoopEliminatePass::MakeComparison(mlir::Value *lhs, mlir::Value *rhs, mlir::Block *at) -> mlir::Value * {
  auto *result = GET_MLIR_CONTEXT()->CreateValue();
  result->SetType(type::TypeFactory::GetBooleanType());
  auto *operation = GET_MLIR_CONTEXT()->CreateOperation<CompareOp>(result, lhs, rhs);
  lhs->AddUse(operation, 0);
  rhs->AddUse(operation, 1);
  InsertBeforeTerminator(operation, at);
  return result;
}

auto DeadLoopEliminatePass::MakeBoolToInt(mlir::Value *value, mlir::Block *at) -> mlir::Value * {
  auto *result = GET_MLIR_CONTEXT()->CreateValue();
  result->SetType(type::TypeFactory::GetIntegerType());
  auto *operation = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::Bool2IntOp>(result, value);
  value->AddUse(operation, 0);
  InsertBeforeTerminator(operation, at);
  return result;
}

auto DeadLoopEliminatePass::ExpandRepeatedDiv(const analysis::SCEV *scev, mlir::Block *at) -> mlir::Value * {
  const auto *repeat = static_cast<const analysis::SCEVRepeatedDivExpr *>(scev);
  if (!repeat->IsClosed() || repeat->GetStepShift() <= 0) {
    return nullptr;
  }
  mlir::Value *start = Expand(repeat->GetStart(), at);
  mlir::Value *iteration = Expand(repeat->GetIteration(), at);
  if (start == nullptr || iteration == nullptr) {
    return nullptr;
  }

  // Once the total shift reaches 32, repeated truncating division of an i32
  // value is exactly zero. Compute a validity bit before multiplying by the
  // iteration count, so neither the exponent arithmetic nor the emitted shift
  // can overflow or use an out-of-range shift amount.
  int base_shift = repeat->GetBaseShift();
  if (base_shift >= 32) {
    return MakeConstInt(0, at);
  }
  int iteration_limit = (31 - base_shift) / repeat->GetStepShift() + 1;
  mlir::Value *zero = MakeConstInt(0, at);
  mlir::Value *limit = MakeConstInt(iteration_limit, at);
  mlir::Value *nonnegative = MakeBoolToInt(MakeComparison<mlir::base::GreaterEqualOp>(iteration, zero, at), at);
  mlir::Value *below_limit = MakeBoolToInt(MakeComparison<mlir::base::LessOp>(iteration, limit, at), at);
  mlir::Value *valid = MakeBinary<mlir::base::MulOp>(nonnegative, below_limit, at);
  mlir::Value *safe_iteration = MakeBinary<mlir::base::MulOp>(iteration, valid, at);
  mlir::Value *step = MakeConstInt(repeat->GetStepShift(), at);
  mlir::Value *exponent = MakeBinary<mlir::base::MulOp>(safe_iteration, step, at);
  if (base_shift != 0) {
    exponent = MakeBinary<mlir::base::AddOp>(exponent, MakeConstInt(base_shift, at), at);
  }

  // Signed division by 2^s, truncating toward zero:
  //   (x + sign(x) * (2^s - 1)) >> s
  // Here s is proven in [0, 31]. The final validity mask selects zero for
  // exponents outside the representable i32 range.
  mlir::Value *one = MakeConstInt(1, at);
  mlir::Value *power = MakeBinary<mlir::base::ShlOp>(one, exponent, at);
  mlir::Value *mask = MakeBinary<mlir::base::SubOp>(power, one, at);
  mlir::Value *sign_shift = MakeConstInt(31, at);
  mlir::Value *sign = MakeBinary<mlir::base::LShrOp>(start, sign_shift, at);
  mlir::Value *bias = MakeBinary<mlir::base::MulOp>(sign, mask, at);
  mlir::Value *adjusted = MakeBinary<mlir::base::AddOp>(start, bias, at);
  mlir::Value *quotient = MakeBinary<mlir::base::AShrOp>(adjusted, exponent, at);
  return MakeBinary<mlir::base::MulOp>(quotient, valid, at);
}

auto DeadLoopEliminatePass::ExpandModAddRec(const analysis::SCEV *scev, mlir::Block *at) -> mlir::Value * {
  const auto *mod_rec = static_cast<const analysis::SCEVModAddRecExpr *>(scev);
  if (!mod_rec->IsClosed()) {
    return nullptr;
  }
  const int step = mod_rec->GetStep();
  const int modulus = mod_rec->GetModulus();
  const int fold = mod_rec->GetFold();
  const int offset = mod_rec->GetOffset();
  if (step <= 0 || step >= modulus || fold <= 0 || offset < 0) {
    return nullptr;
  }
  // The total step count is `fold * iteration + offset`. `k_steps` of them put the state in
  // [0, modulus) from any start; past that the orbit repeats with `period`. `cap` is the smallest
  // iteration count that covers the climb, and clamping to it keeps every product below `bound`.
  // The i32 fit is checked here rather than assumed: if it does not hold, decline to close the loop.
  const int64_t k_steps = (static_cast<int64_t>(modulus) + step - 1) / step;
  const int64_t period = modulus / std::gcd(step, modulus);
  const int64_t needed = k_steps + 1 - offset;
  const int64_t cap = needed <= 0 ? 0 : (needed + fold - 1) / fold;
  int64_t bound = static_cast<int64_t>(modulus) + static_cast<int64_t>(step) * (k_steps + period);
  bound = std::max(bound, static_cast<int64_t>(fold) * cap + offset + 1);
  bound = std::max(bound, static_cast<int64_t>(fold) * (period - 1) + offset + 1 + k_steps + period);
  if (bound > INT32_MAX) {
    return nullptr;
  }

  mlir::Value *start = Expand(mod_rec->GetStart(), at);
  mlir::Value *iteration = Expand(mod_rec->GetIteration(), at);
  if (start == nullptr || iteration == nullptr) {
    return nullptr;
  }

  mlir::Value *zero = MakeConstInt(0, at);
  mlir::Value *one = MakeConstInt(1, at);
  mlir::Value *step_value = MakeConstInt(step, at);
  mlir::Value *modulus_value = MakeConstInt(modulus, at);
  mlir::Value *fold_value = MakeConstInt(fold, at);
  mlir::Value *offset_value = MakeConstInt(offset, at);
  // Every reduction below assumes a non-negative count, so clamp once here rather than resting on
  // how the caller derived the iteration expression. The same predicate selects the untouched start
  // value at the end, so it costs one comparison overall.
  mlir::Value *entered = MakeBoolToInt(MakeComparison<mlir::base::GreaterOp>(iteration, zero, at), at);
  iteration = MakeBinary<mlir::base::MulOp>(iteration, entered, at);
  mlir::Value *k_value = MakeConstInt(static_cast<int>(k_steps), at);
  mlir::Value *period_value = MakeConstInt(static_cast<int>(period), at);
  mlir::Value *cap_value = MakeConstInt(static_cast<int>(cap), at);

  // Phase A -- the additive climb. Clamping the iteration count to `cap` before scaling by `fold`
  // is what makes the product overflow-free; the climb then runs at most `k_steps` steps past the
  // first. While the state is still negative no reduction has happened yet, so the `% modulus` is
  // applied only once it has turned non-negative.
  mlir::Value *below_cap = MakeBoolToInt(MakeComparison<mlir::base::LessOp>(iteration, cap_value, at), at);
  mlir::Value *at_cap = MakeBinary<mlir::base::SubOp>(one, below_cap, at);
  mlir::Value *capped = MakeBinary<mlir::base::AddOp>(MakeBinary<mlir::base::MulOp>(below_cap, iteration, at),
                                                      MakeBinary<mlir::base::MulOp>(at_cap, cap_value, at), at);
  mlir::Value *climb = MakeBinary<mlir::base::SubOp>(
      MakeBinary<mlir::base::AddOp>(MakeBinary<mlir::base::MulOp>(fold_value, capped, at), offset_value, at), one, at);
  mlir::Value *short_climb = MakeBoolToInt(MakeComparison<mlir::base::LessOp>(climb, k_value, at), at);
  mlir::Value *long_climb = MakeBinary<mlir::base::SubOp>(one, short_climb, at);
  climb = MakeBinary<mlir::base::AddOp>(MakeBinary<mlir::base::MulOp>(short_climb, climb, at),
                                        MakeBinary<mlir::base::MulOp>(long_climb, k_value, at), at);
  mlir::Value *first =
      MakeBinary<mlir::base::ModOp>(MakeBinary<mlir::base::AddOp>(start, step_value, at), modulus_value, at);
  mlir::Value *raw = MakeBinary<mlir::base::AddOp>(first, MakeBinary<mlir::base::MulOp>(step_value, climb, at), at);
  mlir::Value *nonnegative = MakeBoolToInt(MakeComparison<mlir::base::GreaterEqualOp>(raw, zero, at), at);
  mlir::Value *negative = MakeBinary<mlir::base::SubOp>(one, nonnegative, at);
  mlir::Value *climbed = MakeBinary<mlir::base::AddOp>(
      MakeBinary<mlir::base::MulOp>(nonnegative, MakeBinary<mlir::base::ModOp>(raw, modulus_value, at), at),
      MakeBinary<mlir::base::MulOp>(negative, raw, at), at);

  // Phase B -- the periodic tail. `iteration % period` keeps the remaining-step count small; the
  // second modulo normalizes truncated-`%` output into [0, period) so the final reduction sees a
  // non-negative operand.
  mlir::Value *residue = MakeBinary<mlir::base::ModOp>(iteration, period_value, at);
  mlir::Value *tail = MakeBinary<mlir::base::SubOp>(
      MakeBinary<mlir::base::SubOp>(
          MakeBinary<mlir::base::AddOp>(MakeBinary<mlir::base::MulOp>(fold_value, residue, at), offset_value, at), one,
          at),
      k_value, at);
  tail = MakeBinary<mlir::base::ModOp>(tail, period_value, at);
  tail = MakeBinary<mlir::base::ModOp>(MakeBinary<mlir::base::AddOp>(tail, period_value, at), period_value, at);
  mlir::Value *wrapped = MakeBinary<mlir::base::ModOp>(
      MakeBinary<mlir::base::AddOp>(climbed, MakeBinary<mlir::base::MulOp>(step_value, tail, at), at), modulus_value,
      at);

  // The tail applies exactly when the real step count runs past the climb, i.e. iteration > cap, or
  // iteration == cap and the capped climb already overshot -- a compile-time fact. Both comparisons
  // are against the clamped count, so neither can overflow.
  mlir::Value *past_cap = MakeBoolToInt(MakeComparison<mlir::base::GreaterOp>(iteration, cap_value, at), at);
  mlir::Value *equals_cap = MakeBoolToInt(MakeComparison<mlir::base::EqualOp>(iteration, cap_value, at), at);
  mlir::Value *cap_overshoots = MakeConstInt(fold * cap + offset - 1 > k_steps ? 1 : 0, at);
  mlir::Value *beyond =
      MakeBinary<mlir::base::AddOp>(past_cap, MakeBinary<mlir::base::MulOp>(equals_cap, cap_overshoots, at), at);
  mlir::Value *within = MakeBinary<mlir::base::SubOp>(one, beyond, at);
  mlir::Value *advanced = MakeBinary<mlir::base::AddOp>(MakeBinary<mlir::base::MulOp>(beyond, wrapped, at),
                                                        MakeBinary<mlir::base::MulOp>(within, climbed, at), at);

  // Zero total steps leaves the start value untouched -- possible when the loop does not run and the
  // value is the header phi itself (offset zero). Selecting arithmetically keeps the whole closed
  // form a straight-line preheader computation.
  if (offset > 0) {
    return advanced;
  }
  mlir::Value *skipped = MakeBinary<mlir::base::SubOp>(one, entered, at);
  return MakeBinary<mlir::base::AddOp>(MakeBinary<mlir::base::MulOp>(entered, advanced, at),
                                       MakeBinary<mlir::base::MulOp>(skipped, start, at), at);
}

auto DeadLoopEliminatePass::Expand(const analysis::SCEV *scev, mlir::Block *at) -> mlir::Value * {
  switch (scev->GetKind()) {
    case analysis::SCEVKind::Constant: {
      const auto *constant = static_cast<const analysis::SCEVConstant *>(scev);
      return constant->IsInt() ? MakeConstInt(constant->GetInt(), at) : nullptr;
    }
    case analysis::SCEVKind::Unknown:
      return static_cast<const analysis::SCEVUnknown *>(scev)->GetValue();
    case analysis::SCEVKind::Add:
    case analysis::SCEVKind::Mul: {
      const auto &operands = static_cast<const analysis::SCEVCommutativeExpr *>(scev)->GetOperands();
      mlir::Value *accumulator = nullptr;
      for (const auto *operand : operands) {
        mlir::Value *expanded = Expand(operand, at);
        if (expanded == nullptr) {
          return nullptr;
        }
        if (accumulator == nullptr) {
          accumulator = expanded;
        } else if (scev->GetKind() == analysis::SCEVKind::Add) {
          accumulator = MakeBinary<mlir::base::AddOp>(accumulator, expanded, at);
        } else {
          accumulator = MakeBinary<mlir::base::MulOp>(accumulator, expanded, at);
        }
      }
      return accumulator;
    }
    case analysis::SCEVKind::Div: {
      const auto *div = static_cast<const analysis::SCEVDivExpr *>(scev);
      mlir::Value *lhs = Expand(div->GetLHS(), at);
      mlir::Value *rhs = Expand(div->GetRHS(), at);
      if (lhs == nullptr || rhs == nullptr) {
        return nullptr;
      }
      return MakeBinary<mlir::base::DivOp>(lhs, rhs, at);
    }
    case analysis::SCEVKind::SMax: {
      const auto &operands = static_cast<const analysis::SCEVSMaxExpr *>(scev)->GetOperands();
      if (operands.size() != 2) {
        return nullptr;
      }
      const analysis::SCEV *value_scev = nullptr;
      for (size_t index = 0; index < 2; ++index) {
        const auto *candidate = operands[index];
        if (candidate->GetKind() == analysis::SCEVKind::Constant &&
            static_cast<const analysis::SCEVConstant *>(candidate)->IsInt() &&
            static_cast<const analysis::SCEVConstant *>(candidate)->GetInt() == 0) {
          value_scev = operands[1 - index];
          break;
        }
      }
      mlir::Value *value = value_scev == nullptr ? nullptr : Expand(value_scev, at);
      if (value == nullptr) {
        return nullptr;
      }
      mlir::Value *zero = MakeConstInt(0, at);
      mlir::Value *nonnegative = MakeBoolToInt(MakeComparison<mlir::base::GreaterEqualOp>(value, zero, at), at);
      return MakeBinary<mlir::base::MulOp>(value, nonnegative, at);
    }
    case analysis::SCEVKind::Mod: {
      const auto *mod = static_cast<const analysis::SCEVModExpr *>(scev);
      mlir::Value *lhs = Expand(mod->GetLHS(), at);
      mlir::Value *rhs = Expand(mod->GetRHS(), at);
      if (lhs == nullptr || rhs == nullptr) {
        return nullptr;
      }
      return MakeBinary<mlir::base::ModOp>(lhs, rhs, at);
    }
    case analysis::SCEVKind::RepeatedDiv:
      return ExpandRepeatedDiv(scev, at);
    case analysis::SCEVKind::ModAddRec:
      return ExpandModAddRec(scev, at);
    default:
      return nullptr;
  }
}

}  // namespace middleend::pass::transform
