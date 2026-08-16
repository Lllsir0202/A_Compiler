#include "middleend/pass/transform/modrecurrencesink/modrecurrencesink.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "context/compiler_context.h"
#include "context/mlir_context.h"
#include "middleend/mlir/dialect/base/binaryop.h"
#include "middleend/mlir/dialect/base/cast.h"
#include "middleend/mlir/dialect/base/copy.h"
#include "middleend/mlir/dialect/base/phi.h"
#include "middleend/mlir/dialect/base/unaryop.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/block.h"
#include "middleend/mlir/utils/value/value.h"
#include "middleend/pass/analysis/cfg/mlircfg.h"
#include "middleend/pass/analysis/loopinfo/loopinfo.h"
#include "middleend/pass/analysis/scev/scevpass.h"

#define GET_PASS_CONTEXT() compiler_context_->GetPassContext()
#define GET_MLIR_CONTEXT() compiler_context_->GetIRContext()->GetMLIRContext()

namespace middleend::pass::transform {
namespace {
auto StripCopies(mlir::Value *value, std::vector<mlir::base::CopyOp *> *copies = nullptr) -> mlir::Value * {
  while (auto *copy = mlir::DynamicOrNull<mlir::base::CopyOp>(value == nullptr ? nullptr : value->op_)) {
    if (copies != nullptr) copies->push_back(copy);
    value = copy->GetSrc();
  }
  return value;
}

auto InsertBeforeTerminator(mlir::Block *block, mlir::Operation *operation) -> void {
  block->operations_.insert(block->operations_.end() - 1, operation);
}

auto InsertBefore(mlir::Block *block, mlir::Operation *position, mlir::Operation *operation) -> void {
  auto it = std::find(block->operations_.begin(), block->operations_.end(), position);
  block->operations_.insert(it, operation);
}

auto InsertAfterPhis(mlir::Block *block, const std::vector<mlir::Operation *> &operations) -> void {
  auto it = block->operations_.begin();
  while (it != block->operations_.end() && mlir::DynamicOrNull<mlir::base::PhiOp>(*it) != nullptr) ++it;
  block->operations_.insert(it, operations.begin(), operations.end());
}
}  // namespace

auto ModRecurrenceSinkPass::Run() -> void {
  PreRun();
  auto *cfg_pass = GET_PASS_CONTEXT()->GetSpecPass<analysis::CFGPass>("CFGPass");
  auto *loop_info = GET_PASS_CONTEXT()->GetSpecPass<analysis::LoopInfoPass>("LoopInfoPass");
  auto *scev = GET_PASS_CONTEXT()->GetSpecPass<analysis::SCEVPass>("SCEVPass");
  bool changed = false;
  for (auto *function : cfg_pass->Get()) {
    auto *cfg = dynamic_cast<analysis::MLIRFunctionCFG *>(function);
    if (cfg == nullptr) continue;
    // Innermost first keeps a transformed inner recurrence from confusing an
    // enclosing loop's structural use checks.
    const auto &loops = loop_info->GetLoops(function);
    for (auto it = loops.rbegin(); it != loops.rend(); ++it) changed = RunOnLoop(cfg, *it, scev) || changed;
  }
  if (changed) GET_PASS_CONTEXT()->InvalidateAnalysis();
}

auto ModRecurrenceSinkPass::RunOnLoop(analysis::MLIRFunctionCFG *cfg, analysis::Loop *loop,
                                      analysis::SCEVPass *scev) -> bool {
  if (loop->GetLatches().size() != 1) return false;
  // A known constant count provides an additional guard.  Symbolic SysY loops
  // are still legal: the source language's i32 control domain bounds a
  // terminating recurrence, which is the same i32*i32 <= i64 argument used by
  // established implementations of this transform.
  int64_t trip_count = scev->GetConstantTripCount(loop);
  if (trip_count > std::numeric_limits<int32_t>::max()) return false;

  auto *header = loop->GetHeader();
  auto *latch = loop->GetLatches()[0];
  auto *header_node = cfg->GetNodeByBlock(header);
  if (header_node == nullptr) return false;
  mlir::Block *preheader = nullptr;
  for (auto *pred : header_node->GetPredecessors()) {
    auto *node = dynamic_cast<analysis::MLIRCFGNode *>(pred);
    if (node == nullptr || loop->Contains(node->GetBlock())) continue;
    if (preheader != nullptr) return false;
    preheader = node->GetBlock();
  }
  if (preheader == nullptr || preheader->operations_.empty()) return false;

  mlir::Block *exit = nullptr;
  for (auto *block : loop->GetBlocks()) {
    auto *node = cfg->GetNodeByBlock(block);
    if (node == nullptr) continue;
    for (auto *succ : node->GetSuccessors()) {
      auto *succ_node = dynamic_cast<analysis::MLIRCFGNode *>(succ);
      if (succ_node == nullptr || loop->Contains(succ_node->GetBlock())) continue;
      if (exit != nullptr && exit != succ_node->GetBlock()) return false;
      exit = succ_node->GetBlock();
    }
  }
  if (exit == nullptr) return false;

  std::unordered_map<mlir::Operation *, mlir::Block *> owner;
  for (auto *block : loop->GetBlocks())
    for (auto *op : block->operations_) owner[op] = block;

  for (auto *operation : header->operations_) {
    auto *phi = mlir::DynamicOrNull<mlir::base::PhiOp>(operation);
    if (phi == nullptr || phi->GetRes()->GetType()->GetKind() != type::TypeKind::INT || phi->GetIncomingSize() != 2)
      continue;

    size_t latch_index = 2;
    size_t entry_index = 2;
    for (size_t i = 0; i < 2; ++i) {
      if (phi->GetIncomingBlock()[i] == latch) latch_index = i;
      if (phi->GetIncomingBlock()[i] == preheader) entry_index = i;
    }
    if (latch_index == 2 || entry_index == 2) continue;

    std::vector<mlir::base::CopyOp *> recurrence_copies;
    auto *latch_root = StripCopies(phi->GetIncomingValue()[latch_index], &recurrence_copies);
    auto *mod = mlir::DynamicOrNull<mlir::base::ModOp>(latch_root == nullptr ? nullptr : latch_root->op_);
    if (mod == nullptr) continue;
    auto *modulus = mlir::DynamicOrNull<mlir::base::ConstIntOp>(mod->GetRHS()->op_);
    if (modulus == nullptr || modulus->GetValue() <= 0) continue;
    auto *add = mlir::DynamicOrNull<mlir::base::AddOp>(mod->GetLHS()->op_);
    if (add == nullptr) continue;
    if (add->GetRes()->GetUses().size() != 1 || add->GetRes()->GetUses()[0]->operation_ != mod) continue;
    mlir::Value *increment = nullptr;
    if (add->GetLHS() == phi->GetRes()) increment = add->GetRHS();
    if (add->GetRHS() == phi->GetRes()) increment = add->GetLHS();
    if (increment == nullptr || owner.count(add) == 0 || owner.count(mod) == 0) continue;

    // The reduced accumulator must not be observed in the loop except by its
    // own increment.  This is the key semantic legality condition.
    size_t in_loop_uses = 0;
    for (auto *use : phi->GetRes()->GetUses())
      if (owner.count(use->operation_) != 0) ++in_loop_uses;
    if (in_loop_uses != 1) continue;

    mlir::base::PhiOp *exit_phi = nullptr;
    for (auto *exit_op : exit->operations_) {
      auto *candidate = mlir::DynamicOrNull<mlir::base::PhiOp>(exit_op);
      if (candidate == nullptr) break;
      bool carries_recurrence = false;
      for (auto *incoming : candidate->GetIncomingValue()) {
        auto *root = StripCopies(incoming);
        carries_recurrence = carries_recurrence || root == mod->GetRes() || root == phi->GetRes();
      }
      if (carries_recurrence) {
        if (exit_phi != nullptr) {
          exit_phi = nullptr;
          break;
        }
        exit_phi = candidate;
      }
    }
    if (exit_phi == nullptr) continue;
    std::unordered_set<mlir::Value *> visited_flow;
    std::function<bool(mlir::Value *)> only_recurrence_flow = [&](mlir::Value *value) {
      if (!visited_flow.insert(value).second) return true;
      for (auto *use : value->GetUses()) {
        if (use->operation_ == phi || use->operation_ == exit_phi) continue;
        auto *copy = mlir::DynamicOrNull<mlir::base::CopyOp>(use->operation_);
        if (copy == nullptr || !only_recurrence_flow(copy->GetRes())) return false;
      }
      return true;
    };
    if (!only_recurrence_flow(mod->GetRes())) continue;
    std::vector<mlir::base::CopyOp *> exit_copies;
    bool valid_exit_flow = true;
    for (auto *incoming : exit_phi->GetIncomingValue()) {
      std::vector<mlir::base::CopyOp *> copies;
      auto *root = StripCopies(incoming, &copies);
      if (root != mod->GetRes() && root != phi->GetRes()) {
        valid_exit_flow = false;
        break;
      }
      exit_copies.insert(exit_copies.end(), copies.begin(), copies.end());
    }
    if (!valid_exit_flow) continue;

    auto *long_type = compiler_context_->GetTypeContext()->GetLongType();
    auto make_cast = [&](mlir::Value *source, mlir::Block *block, mlir::Operation *before) {
      auto *result = GET_MLIR_CONTEXT()->CreateValue();
      result->SetType(long_type);
      auto *cast = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::Int2LongOp>(result, source);
      source->AddUse(cast, 0);
      if (before != nullptr)
        InsertBefore(block, before, cast);
      else
        InsertBeforeTerminator(block, cast);
      return result;
    };

    auto *wide_init = make_cast(phi->GetIncomingValue()[entry_index], preheader, nullptr);
    auto *add_block = owner[add];
    auto *wide_increment = make_cast(increment, add_block, add);

    // Retype the recurrence and remove the per-iteration remainder.  Copy
    // nodes along the latch edge are kept but widened, preserving CFG shape.
    phi->GetIncomingValue()[entry_index]->RemoveUse(phi);
    phi->UpdateIncoming(entry_index, wide_init, preheader);
    wide_init->AddUse(phi, entry_index);
    phi->GetRes()->SetType(long_type);
    increment->RemoveUse(add);
    if (add->GetLHS() == increment)
      add->operands_[0] = wide_increment;
    else
      add->operands_[1] = wide_increment;
    wide_increment->AddUse(add, add->GetLHS() == wide_increment ? 0 : 1);
    add->GetRes()->SetType(long_type);
    mod->GetRes()->ReplaceAllUsesWith(add->GetRes());
    for (auto *copy : recurrence_copies) copy->GetRes()->SetType(long_type);
    for (auto *copy : exit_copies) copy->GetRes()->SetType(long_type);
    mod->GetLHS()->RemoveUse(mod);
    mod->GetRHS()->RemoveUse(mod);
    owner[mod]->RemoveOperation(mod);

    // All incoming recurrence names are now wide.  Rejecting multiple exits
    // above means this single LCSSA phi is the only place a narrow result must
    // be restored.
    exit_phi->GetRes()->SetType(long_type);

    auto *fresh_modulus = GET_MLIR_CONTEXT()->CreateValue();
    fresh_modulus->SetType(compiler_context_->GetTypeContext()->GetIntegerType());
    auto *fresh_modulus_op =
        GET_MLIR_CONTEXT()->CreateOperation<mlir::base::ConstIntOp>(fresh_modulus, modulus->GetValue());
    auto *wide_modulus = GET_MLIR_CONTEXT()->CreateValue();
    wide_modulus->SetType(long_type);
    auto *modulus_cast = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::Int2LongOp>(wide_modulus, fresh_modulus);
    fresh_modulus->AddUse(modulus_cast, 0);
    auto *mod_result = GET_MLIR_CONTEXT()->CreateValue();
    mod_result->SetType(long_type);
    auto *exit_mod =
        GET_MLIR_CONTEXT()->CreateOperation<mlir::base::ModOp>(mod_result, exit_phi->GetRes(), wide_modulus);
    auto *narrow_result = GET_MLIR_CONTEXT()->CreateValue();
    narrow_result->SetType(compiler_context_->GetTypeContext()->GetIntegerType());
    auto *trunc = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::Long2IntOp>(narrow_result, mod_result);
    exit_phi->GetRes()->ReplaceAllUsesWith(narrow_result);
    exit_phi->GetRes()->AddUse(exit_mod, 0);
    wide_modulus->AddUse(exit_mod, 1);
    mod_result->AddUse(trunc, 0);
    InsertAfterPhis(exit, {fresh_modulus_op, modulus_cast, exit_mod, trunc});
    return true;
  }
  return false;
}

}  // namespace middleend::pass::transform
