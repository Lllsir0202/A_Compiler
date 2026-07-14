#include "middleend/pass/transform/licm/licm.h"
#include <algorithm>
#include <stdexcept>
#include <unordered_set>
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/base/binaryop.h"
#include "middleend/mlir/dialect/base/cast.h"
#include "middleend/mlir/dialect/base/funccall.h"
#include "middleend/mlir/dialect/base/unaryop.h"
#include "middleend/mlir/dialect/func/funccall.h"
#include "middleend/mlir/dialect/var/getptr.h"
#include "middleend/mlir/dialect/var/getvalue.h"
#include "middleend/mlir/dialect/var/set.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/block.h"
#include "middleend/mlir/utils/value/value.h"
#include "middleend/pass/analysis/alias/alias.h"
#include "middleend/pass/analysis/cfg/cfg.h"
#include "middleend/pass/analysis/cfg/mlircfg.h"
#include "middleend/pass/analysis/loopinfo/loopinfo.h"

#define GET_PASS_CONTEXT() compiler_context_->GetPassContext()

namespace middleend::pass::transform {
namespace {
template <typename OperationType>
auto Is(mlir::Operation *operation) -> bool {
  return mlir::DynamicOrNull<OperationType>(operation) != nullptr;
}

auto IsStore(mlir::Operation *operation) -> bool { return Is<mlir::var::VarSetOp>(operation); }
auto IsCall(mlir::Operation *operation) -> bool {
  return Is<mlir::base::FuncCallOp>(operation) || Is<mlir::func::FuncCallOp>(operation);
}
auto IsLoad(mlir::Operation *operation) -> bool { return Is<mlir::var::VarGetValOp>(operation); }
}  // namespace

auto LICMPass::Run() -> void {
  PreRun();
  auto *cfg_pass = GET_PASS_CONTEXT()->GetSpecPass<analysis::CFGPass>("CFGPass");
  auto *loop_info = GET_PASS_CONTEXT()->GetSpecPass<analysis::LoopInfoPass>("LoopInfoPass");
  alias_analysis_ = GET_PASS_CONTEXT()->GetSpecPass<analysis::AliasAnalysisPass>("AliasAnalysisPass");
  if (cfg_pass == nullptr || loop_info == nullptr || alias_analysis_ == nullptr) {
    throw std::logic_error("LICMPass requires CFGPass, LoopInfoPass and AliasAnalysisPass");
  }

  bool changed = false;
  for (auto *function_cfg : cfg_pass->Get()) {
    auto *mlir_function_cfg = dynamic_cast<analysis::MLIRFunctionCFG *>(function_cfg);
    if (mlir_function_cfg == nullptr) {
      throw std::logic_error("LICMPass only supports MLIRFunctionCFG");
    }

    // Map every operation to its block; hoisting keeps this current so invariance queries see a
    // hoisted op as living outside the loop.
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

    // Innermost loops first: a value hoisted out of an inner loop may then be invariant in the
    // enclosing one.
    std::vector<analysis::Loop *> loops = loop_info->GetLoops(function_cfg);
    std::sort(loops.begin(), loops.end(),
              [](analysis::Loop *lhs, analysis::Loop *rhs) { return lhs->GetDepth() > rhs->GetDepth(); });
    for (auto *loop : loops) {
      changed = RunOnLoop(mlir_function_cfg, loop) || changed;
    }
  }

  if (changed) {
    GET_PASS_CONTEXT()->InvalidateAnalysis();
  }
}

auto LICMPass::FindPreheader(analysis::MLIRFunctionCFG *function_cfg, analysis::Loop *loop) -> mlir::Block * {
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

auto LICMPass::RunOnLoop(analysis::MLIRFunctionCFG *function_cfg, analysis::Loop *loop) -> bool {
  auto *preheader = FindPreheader(function_cfg, loop);
  if (preheader == nullptr || preheader->operations_.empty()) {
    return false;
  }

  // Memory writes of the loop are fixed (stores/calls are never hoisted); collect them once so a
  // load can be tested against them for invariance.
  std::vector<mlir::Operation *> loop_stores;
  std::vector<mlir::Operation *> loop_calls;
  for (auto *block : loop->GetBlocks()) {
    for (auto *operation : block->operations_) {
      if (IsStore(operation)) {
        loop_stores.emplace_back(operation);
      } else if (IsCall(operation)) {
        loop_calls.emplace_back(operation);
      }
    }
  }

  bool changed = false;
  bool moved = true;
  while (moved) {
    moved = false;
    for (auto *block : loop->GetBlocks()) {
      // The header is processed like any other loop block: its phis are never hoistable (not in
      // the pure whitelist), while a genuinely invariant computation in it still moves out.
      std::vector<mlir::Operation *> hoisted;
      for (auto *operation : block->operations_) {
        if (IsHoistable(operation, loop, loop_stores, loop_calls)) {
          hoisted.emplace_back(operation);
        }
      }
      if (hoisted.empty()) {
        continue;
      }
      std::unordered_set<mlir::Operation *> hoisted_set(hoisted.begin(), hoisted.end());
      std::vector<mlir::Operation *> kept;
      kept.reserve(block->operations_.size());
      for (auto *operation : block->operations_) {
        if (hoisted_set.count(operation) == 0) {
          kept.emplace_back(operation);
        }
      }
      block->operations_ = std::move(kept);
      for (auto *operation : hoisted) {
        Hoist(operation, preheader);
      }
      changed = true;
      moved = true;
    }
  }
  return changed;
}

auto LICMPass::IsInvariant(mlir::Value *value, analysis::Loop *loop) -> bool {
  if (value == nullptr) {
    return true;
  }
  if (value->op_ == nullptr) {
    return true;  // parameters, globals and other externally-defined values
  }
  auto iter = op_to_block_.find(value->op_);
  if (iter == op_to_block_.end()) {
    return true;  // defined by an operation outside any tracked block
  }
  return !loop->Contains(iter->second);
}

auto LICMPass::IsHoistable(mlir::Operation *operation, analysis::Loop *loop,
                           const std::vector<mlir::Operation *> &loop_stores,
                           const std::vector<mlir::Operation *> &loop_calls) -> bool {
  if (operation->results_.size() != 1) {
    return false;
  }

  if (IsLoad(operation)) {
    auto *load_op = mlir::DynamicOrNull<mlir::var::VarGetValOp>(operation);
    auto *pointer = load_op->GetPtr();
    if (!IsInvariant(pointer, loop)) {
      return false;
    }
    // The loaded value is loop-invariant only if nothing in the loop may write the location.
    bool no_store_aliases = std::all_of(loop_stores.begin(), loop_stores.end(), [&](mlir::Operation *store) {
      auto *store_op = mlir::DynamicOrNull<mlir::var::VarSetOp>(store);
      return alias_analysis_->Alias(pointer, store_op->GetPtr()) == analysis::AliasResult::NoAlias;
    });
    bool no_call_modifies = std::all_of(loop_calls.begin(), loop_calls.end(), [&](mlir::Operation *call) {
      return !alias_analysis_->CallModRef(call, pointer);
    });
    return no_store_aliases && no_call_modifies;
  }

  if (!IsSpeculatablePure(operation)) {
    return false;
  }
  return std::all_of(operation->operands_.begin(), operation->operands_.end(),
                     [&](mlir::Value *operand) { return IsInvariant(operand, loop); });
}

auto LICMPass::IsSpeculatablePure(mlir::Operation *operation) -> bool {
  // Integer arithmetic and comparisons. Integer div/mod are included: unlike x86, both backend
  // targets define division by zero (RISC-V yields all-ones / the dividend; AArch64 yields 0) and
  // INT_MIN/-1 (wraps to INT_MIN) without trapping, so an invariant div/mod is safe to execute
  // speculatively in the preheader even when the loop body runs zero times.
  if (Is<mlir::base::AddOp>(operation) || Is<mlir::base::SubOp>(operation) || Is<mlir::base::MulOp>(operation) ||
      Is<mlir::base::DivOp>(operation) || Is<mlir::base::ModOp>(operation) || Is<mlir::base::EqualOp>(operation) ||
      Is<mlir::base::NotEqualOp>(operation) || Is<mlir::base::LessOp>(operation) ||
      Is<mlir::base::LessEqualOp>(operation) || Is<mlir::base::GreaterOp>(operation) ||
      Is<mlir::base::GreaterEqualOp>(operation)) {
    return true;
  }
  // Floating-point arithmetic and comparisons (IEEE division does not trap).
  if (Is<mlir::base::AddFOp>(operation) || Is<mlir::base::SubFOp>(operation) || Is<mlir::base::MulFOp>(operation) ||
      Is<mlir::base::DivFOp>(operation) || Is<mlir::base::EqualFOp>(operation) ||
      Is<mlir::base::NotEqualFOp>(operation) || Is<mlir::base::LessFOp>(operation) ||
      Is<mlir::base::LessEqualFOp>(operation) || Is<mlir::base::GreaterFOp>(operation) ||
      Is<mlir::base::GreaterEqualFOp>(operation)) {
    return true;
  }
  // Unary operations.
  if (Is<mlir::base::NegOp>(operation) || Is<mlir::base::NegFOp>(operation) || Is<mlir::base::NotOp>(operation)) {
    return true;
  }
  // Type casts.
  if (Is<mlir::base::Int2FloatOp>(operation) || Is<mlir::base::Float2IntOp>(operation) ||
      Is<mlir::base::Bool2IntOp>(operation) || Is<mlir::base::Int2BoolOp>(operation) ||
      Is<mlir::base::Bool2FloatOp>(operation) || Is<mlir::base::Float2BoolOp>(operation)) {
    return true;
  }
  // Constants.
  if (Is<mlir::base::ConstIntOp>(operation) || Is<mlir::base::ConstFloatOp>(operation)) {
    return true;
  }
  // Address computation: pure, and a loop-invariant address is worth hoisting for the load it feeds.
  if (Is<mlir::var::VarGetPtrOp>(operation)) {
    return true;
  }
  return false;
}

auto LICMPass::Hoist(mlir::Operation *operation, mlir::Block *preheader) -> void {
  // Insert just before the preheader terminator, after any previously hoisted operations, so that
  // a hoisted definition still precedes the hoisted use that depends on it.
  auto &operations = preheader->operations_;
  operations.insert(operations.end() - 1, operation);
  op_to_block_[operation] = preheader;
}

}  // namespace middleend::pass::transform
