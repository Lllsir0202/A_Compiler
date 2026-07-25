#include "middleend/pass/transform/deadloopelim/deadloopelim.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
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
    }
  }

  if (std::getenv("DLE_STATS") != nullptr) {
    std::fprintf(stderr, "[DLE] deleted loops: %d\n", deleted_count_);
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
    return false;
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
                                static_cast<const analysis::SCEVRepeatedDivExpr *>(scev)->GetLoop() == loop);
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
          return false;  // cannot express this live-out in closed form: leave the loop alone
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
    case analysis::SCEVKind::RepeatedDiv:
      return ExpandRepeatedDiv(scev, at);
    default:
      return nullptr;
  }
}

}  // namespace middleend::pass::transform
