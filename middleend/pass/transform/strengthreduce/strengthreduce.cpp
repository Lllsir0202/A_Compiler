#include "middleend/pass/transform/strengthreduce/strengthreduce.h"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <vector>
#include "context/compiler_context.h"
#include "context/mlir_context.h"
#include "middleend/mlir/dialect/base/binaryop.h"
#include "middleend/mlir/dialect/base/phi.h"
#include "middleend/mlir/dialect/base/unaryop.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/block.h"
#include "middleend/mlir/utils/value/value.h"
#include "middleend/pass/analysis/cfg/cfg.h"
#include "middleend/pass/analysis/cfg/mlircfg.h"
#include "middleend/pass/analysis/loopinfo/loopinfo.h"
#include "middleend/pass/analysis/scev/scev.h"
#include "middleend/pass/analysis/scev/scevpass.h"

#define GET_PASS_CONTEXT() compiler_context_->GetPassContext()
#define GET_MLIR_CONTEXT() compiler_context_->GetIRContext()->GetMLIRContext()

namespace middleend::pass::transform {

auto StrengthReducePass::Run() -> void {
  PreRun();
  // Benchmark A/B toggle, matching the DISABLE_LICM convention used elsewhere for measuring a pass.
  if (std::getenv("SR_OFF") != nullptr) {
    return;
  }
  auto *cfg_pass = GET_PASS_CONTEXT()->GetSpecPass<analysis::CFGPass>("CFGPass");
  auto *loop_info = GET_PASS_CONTEXT()->GetSpecPass<analysis::LoopInfoPass>("LoopInfoPass");
  scev_ = GET_PASS_CONTEXT()->GetSpecPass<analysis::SCEVPass>("SCEVPass");
  if (cfg_pass == nullptr || loop_info == nullptr || scev_ == nullptr) {
    throw std::logic_error("StrengthReducePass requires CFGPass, LoopInfoPass and SCEVPass");
  }

  bool changed = false;
  for (auto *function_cfg : cfg_pass->Get()) {
    auto *mlir_function_cfg = dynamic_cast<analysis::MLIRFunctionCFG *>(function_cfg);
    if (mlir_function_cfg == nullptr) {
      throw std::logic_error("StrengthReducePass only supports MLIRFunctionCFG");
    }

    // A value's defining block is needed to decide whether every use of a candidate stays in the
    // loop; build the map once per function.
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

    for (auto *loop : loop_info->GetLoops(function_cfg)) {
      changed = RunOnLoop(mlir_function_cfg, loop) || changed;
    }
  }

  if (std::getenv("SR_STATS") != nullptr) {
    std::fprintf(stderr, "[SR] strength-reduced multiplies: %d\n", reduced_count_);
  }
  if (changed) {
    GET_PASS_CONTEXT()->InvalidateAnalysis();
  }
}

auto StrengthReducePass::FindPreheader(analysis::MLIRFunctionCFG *function_cfg, analysis::Loop *loop) -> mlir::Block * {
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
      continue;  // a latch, not the preheader
    }
    if (preheader != nullptr) {
      return nullptr;  // more than one outside predecessor: no dedicated preheader
    }
    preheader = pred_block;
  }
  return preheader;
}

auto StrengthReducePass::RunOnLoop(analysis::MLIRFunctionCFG *function_cfg, analysis::Loop *loop) -> bool {
  auto *preheader = FindPreheader(function_cfg, loop);
  if (preheader == nullptr || preheader->operations_.empty()) {
    return false;
  }
  const auto &latches = loop->GetLatches();
  if (latches.size() != 1) {
    return false;  // a single latch gives the header phi exactly one back-edge value
  }
  auto *latch = latches[0];
  auto *header = loop->GetHeader();
  if (latch->operations_.empty()) {
    return false;
  }

  // The header phi we build has one incoming per header predecessor, so require the canonical shape
  // where those predecessors are exactly the preheader and the latch.
  auto *header_node = function_cfg->GetNodeByBlock(header);
  if (header_node == nullptr || header_node->GetPredecessors().size() != 2) {
    return false;
  }
  for (auto *predecessor : header_node->GetPredecessors()) {
    auto *mlir_pred = dynamic_cast<analysis::MLIRCFGNode *>(predecessor);
    if (mlir_pred == nullptr) {
      return false;
    }
    if (mlir_pred->GetBlock() != preheader && mlir_pred->GetBlock() != latch) {
      return false;
    }
  }

  bool changed = false;
  // Recurrences are uniqued, so identical multiplies share one reduced induction variable.
  std::unordered_map<const analysis::SCEV *, mlir::Value *> reduced;
  for (auto *block : loop->GetBlocks()) {
    std::vector<mlir::Operation *> snapshot = block->operations_;
    for (auto *operation : snapshot) {
      auto *mul = mlir::DynamicOrNull<mlir::base::MulOp>(operation);
      if (mul == nullptr) {
        continue;
      }
      auto *result = mul->GetRes();
      if (result->GetType() == nullptr || result->GetType()->GetKind() != type::TypeKind::INT) {
        continue;
      }
      const analysis::SCEV *scev = scev_->GetSCEV(result);
      if (scev->GetKind() != analysis::SCEVKind::AddRec) {
        continue;
      }
      const auto *add_rec = static_cast<const analysis::SCEVAddRecExpr *>(scev);
      if (add_rec->GetLoop() != loop) {
        continue;
      }
      const analysis::SCEV *start = add_rec->GetStart();
      const analysis::SCEV *step = add_rec->GetStep();
      if (ContainsAddRec(start) || ContainsAddRec(step)) {
        continue;  // needs an outer induction value to expand: out of scope, left untouched
      }
      if (!scev_->IsLoopInvariant(start, loop) || !scev_->IsLoopInvariant(step, loop)) {
        continue;
      }
      if (!AllUsesInLoop(result, loop)) {
        continue;
      }

      mlir::Value *iv = nullptr;
      auto cached = reduced.find(scev);
      if (cached != reduced.end()) {
        iv = cached->second;
      } else {
        mlir::Value *init = Expand(start, preheader);
        mlir::Value *stride = Expand(step, preheader);
        if (init == nullptr || stride == nullptr) {
          continue;
        }
        // Advance value in the latch: next = phi + stride.
        auto *phi_value = GET_MLIR_CONTEXT()->CreateValue();
        phi_value->SetType(result->GetType());
        auto *next_value = GET_MLIR_CONTEXT()->CreateValue();
        next_value->SetType(result->GetType());
        auto *next_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::AddOp>(next_value, phi_value, stride);
        phi_value->AddUse(next_op, 0);
        stride->AddUse(next_op, 1);
        InsertBeforeTerminator(next_op, latch);

        // Header phi: init from the preheader, next from the latch.
        std::vector<mlir::Value *> values = {init, next_value};
        std::vector<mlir::Block *> blocks = {preheader, latch};
        auto *phi_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::PhiOp>(phi_value, values, blocks);
        init->AddUse(phi_op, 0);
        next_value->AddUse(phi_op, 1);
        header->operations_.insert(header->operations_.begin(), phi_op);
        op_to_block_[phi_op] = header;
        op_to_block_[next_op] = latch;

        iv = phi_value;
        reduced[scev] = iv;
      }

      result->ReplaceAllUsesWith(iv);
      for (auto *operand : mul->operands_) {
        operand->RemoveUse(mul);
      }
      block->RemoveOperation(mul);
      op_to_block_.erase(mul);
      ++reduced_count_;
      changed = true;
    }
  }
  return changed;
}

auto StrengthReducePass::ContainsAddRec(const analysis::SCEV *scev) -> bool {
  switch (scev->GetKind()) {
    case analysis::SCEVKind::AddRec:
      return true;
    case analysis::SCEVKind::Add:
    case analysis::SCEVKind::Mul: {
      const auto &operands = static_cast<const analysis::SCEVCommutativeExpr *>(scev)->GetOperands();
      for (const auto *operand : operands) {
        if (ContainsAddRec(operand)) {
          return true;
        }
      }
      return false;
    }
    case analysis::SCEVKind::Div: {
      const auto *div = static_cast<const analysis::SCEVDivExpr *>(scev);
      return ContainsAddRec(div->GetLHS()) || ContainsAddRec(div->GetRHS());
    }
    default:
      return false;
  }
}

auto StrengthReducePass::AllUsesInLoop(mlir::Value *value, analysis::Loop *loop) -> bool {
  for (auto *use : value->GetUses()) {
    auto iter = op_to_block_.find(use->operation_);
    if (iter == op_to_block_.end() || !loop->Contains(iter->second)) {
      return false;
    }
  }
  return true;
}

auto StrengthReducePass::InsertBeforeTerminator(mlir::Operation *operation, mlir::Block *block) -> void {
  block->operations_.insert(block->operations_.end() - 1, operation);
}

auto StrengthReducePass::MakeConstInt(int value, mlir::Block *preheader) -> mlir::Value * {
  auto *result = GET_MLIR_CONTEXT()->CreateValue();
  result->SetType(type::TypeFactory::GetIntegerType());
  auto *op = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::ConstIntOp>(result, value);
  InsertBeforeTerminator(op, preheader);
  op_to_block_[op] = preheader;
  return result;
}

template <typename BinaryOp>
auto StrengthReducePass::MakeBinary(mlir::Value *lhs, mlir::Value *rhs, mlir::Block *preheader) -> mlir::Value * {
  auto *result = GET_MLIR_CONTEXT()->CreateValue();
  result->SetType(lhs->GetType());
  auto *op = GET_MLIR_CONTEXT()->CreateOperation<BinaryOp>(result, lhs, rhs);
  lhs->AddUse(op, 0);
  rhs->AddUse(op, 1);
  InsertBeforeTerminator(op, preheader);
  op_to_block_[op] = preheader;
  return result;
}

auto StrengthReducePass::Expand(const analysis::SCEV *scev, mlir::Block *preheader) -> mlir::Value * {
  switch (scev->GetKind()) {
    case analysis::SCEVKind::Constant: {
      const auto *constant = static_cast<const analysis::SCEVConstant *>(scev);
      if (!constant->IsInt()) {
        return nullptr;
      }
      return MakeConstInt(constant->GetInt(), preheader);
    }
    case analysis::SCEVKind::Unknown: {
      // Loop-invariant leaf: it is defined outside the loop and therefore dominates the preheader,
      // so it is directly usable at the insertion point.
      return static_cast<const analysis::SCEVUnknown *>(scev)->GetValue();
    }
    case analysis::SCEVKind::Add:
    case analysis::SCEVKind::Mul: {
      const auto &operands = static_cast<const analysis::SCEVCommutativeExpr *>(scev)->GetOperands();
      mlir::Value *accumulator = nullptr;
      for (const auto *operand : operands) {
        mlir::Value *expanded = Expand(operand, preheader);
        if (expanded == nullptr) {
          return nullptr;
        }
        if (accumulator == nullptr) {
          accumulator = expanded;
        } else if (scev->GetKind() == analysis::SCEVKind::Add) {
          accumulator = MakeBinary<mlir::base::AddOp>(accumulator, expanded, preheader);
        } else {
          accumulator = MakeBinary<mlir::base::MulOp>(accumulator, expanded, preheader);
        }
      }
      return accumulator;
    }
    case analysis::SCEVKind::Div: {
      const auto *div = static_cast<const analysis::SCEVDivExpr *>(scev);
      mlir::Value *lhs = Expand(div->GetLHS(), preheader);
      mlir::Value *rhs = Expand(div->GetRHS(), preheader);
      if (lhs == nullptr || rhs == nullptr) {
        return nullptr;
      }
      return MakeBinary<mlir::base::DivOp>(lhs, rhs, preheader);
    }
    default:
      return nullptr;  // an AddRec is filtered out before Expand is ever called
  }
}

}  // namespace middleend::pass::transform
