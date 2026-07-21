#include "middleend/pass/transform/looprotate/looprotate.h"
#include <cstdlib>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "context/compiler_context.h"
#include "context/mlir_context.h"
#include "middleend/mlir/dialect/base/binaryop.h"
#include "middleend/mlir/dialect/base/branch.h"
#include "middleend/mlir/dialect/base/funcdef.h"
#include "middleend/mlir/dialect/base/phi.h"
#include "middleend/mlir/dialect/var/getptr.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/block.h"
#include "middleend/mlir/utils/value/value.h"
#include "middleend/pass/analysis/cfg/cfg.h"
#include "middleend/pass/analysis/cfg/mlircfg.h"
#include "middleend/pass/analysis/loopinfo/loopinfo.h"

#define GET_PASS_CONTEXT() compiler_context_->GetPassContext()
#define GET_MLIR_CONTEXT() compiler_context_->GetIRContext()->GetMLIRContext()

namespace middleend::pass::transform {
namespace {

using mlir::Block;
using mlir::DynamicOrNull;
using mlir::Operation;
using mlir::Value;
using mlir::base::BrOp;
using mlir::base::CondBrOp;
using mlir::base::PhiOp;

int rot_block_counter = 0;

auto Terminator(Block *block) -> Operation * {
  return block->operations_.empty() ? nullptr : block->operations_.back();
}

auto SuccessorsOf(Block *block) -> std::vector<Block *> {
  auto *terminator = Terminator(block);
  if (auto *br = DynamicOrNull<BrOp>(terminator)) {
    return {br->GetTarget()};
  }
  if (auto *cond_br = DynamicOrNull<CondBrOp>(terminator)) {
    return {cond_br->GetTrueTarget(), cond_br->GetFalseTarget()};
  }
  return {};
}

auto PredecessorsOf(mlir::base::FuncDefOp *func_op, Block *target) -> std::vector<Block *> {
  std::vector<Block *> preds;
  for (auto *block : func_op->GetBlocks()) {
    for (auto *succ : SuccessorsOf(block)) {
      if (succ == target) {
        preds.emplace_back(block);
        break;
      }
    }
  }
  return preds;
}

// The subset of BASEIR operations that may appear in a loop header and can be
// re-executed once more in the guard without changing semantics: pure integer/
// float arithmetic, comparisons, and address computation. Loads/stores/calls
// are never cloned (a header holding one blocks rotation).
auto IsPureClonable(Operation *op) -> bool {
  return DynamicOrNull<mlir::base::AddOp>(op) != nullptr || DynamicOrNull<mlir::base::SubOp>(op) != nullptr ||
         DynamicOrNull<mlir::base::MulOp>(op) != nullptr || DynamicOrNull<mlir::base::DivOp>(op) != nullptr ||
         DynamicOrNull<mlir::base::ModOp>(op) != nullptr || DynamicOrNull<mlir::base::EqualOp>(op) != nullptr ||
         DynamicOrNull<mlir::base::NotEqualOp>(op) != nullptr || DynamicOrNull<mlir::base::LessOp>(op) != nullptr ||
         DynamicOrNull<mlir::base::LessEqualOp>(op) != nullptr || DynamicOrNull<mlir::base::GreaterOp>(op) != nullptr ||
         DynamicOrNull<mlir::base::GreaterEqualOp>(op) != nullptr || DynamicOrNull<mlir::base::AddFOp>(op) != nullptr ||
         DynamicOrNull<mlir::base::SubFOp>(op) != nullptr || DynamicOrNull<mlir::base::MulFOp>(op) != nullptr ||
         DynamicOrNull<mlir::base::DivFOp>(op) != nullptr || DynamicOrNull<mlir::base::EqualFOp>(op) != nullptr ||
         DynamicOrNull<mlir::base::NotEqualFOp>(op) != nullptr || DynamicOrNull<mlir::base::LessFOp>(op) != nullptr ||
         DynamicOrNull<mlir::base::LessEqualFOp>(op) != nullptr ||
         DynamicOrNull<mlir::base::GreaterFOp>(op) != nullptr ||
         DynamicOrNull<mlir::base::GreaterEqualFOp>(op) != nullptr ||
         DynamicOrNull<mlir::var::VarGetPtrOp>(op) != nullptr;
}

// Clone a pure header op into the preheader guard, remapping operands through
// `value_map` (phi results -> their preheader incoming, earlier clones -> their
// copies). Registers the clone's result in the map and its operand uses.
auto CloneGuardOp(context::MLIRContext *ctx, Operation *op,
                  std::unordered_map<Value *, Value *> *value_map) -> Operation * {
  auto remap = [&](Value *v) -> Value * {
    auto it = value_map->find(v);
    return it == value_map->end() ? v : it->second;
  };

  if (auto *get_ptr = DynamicOrNull<mlir::var::VarGetPtrOp>(op)) {
    std::vector<Value *> indices;
    for (auto *index : get_ptr->GetIndices()) {
      indices.emplace_back(remap(index));
    }
    auto *clone = ctx->CreateOperation<mlir::var::VarGetPtrOp>(ctx, remap(get_ptr->GetPtr()), indices);
    clone->GetRes()->SetType(get_ptr->GetRes()->GetType());
    (*value_map)[get_ptr->GetRes()] = clone->GetRes();
    for (size_t i = 0; i < clone->operands_.size(); ++i) {
      clone->operands_[i]->AddUse(clone, i);
    }
    return clone;
  }

  auto *res = ctx->CreateValue();
  res->SetType(op->results_[0]->GetType());
  res->name_ = op->results_[0]->name_;
  Value *lhs = remap(op->operands_[0]);
  Value *rhs = op->operands_.size() > 1 ? remap(op->operands_[1]) : nullptr;
  Operation *clone = nullptr;

#define ROTATE_CLONE_BINOP(TYPE)                                   \
  if (DynamicOrNull<mlir::base::TYPE>(op) != nullptr) {            \
    clone = ctx->CreateOperation<mlir::base::TYPE>(res, lhs, rhs); \
  }
  ROTATE_CLONE_BINOP(AddOp)
  ROTATE_CLONE_BINOP(SubOp)
  ROTATE_CLONE_BINOP(MulOp)
  ROTATE_CLONE_BINOP(DivOp)
  ROTATE_CLONE_BINOP(ModOp)
  ROTATE_CLONE_BINOP(EqualOp)
  ROTATE_CLONE_BINOP(NotEqualOp)
  ROTATE_CLONE_BINOP(LessOp)
  ROTATE_CLONE_BINOP(LessEqualOp)
  ROTATE_CLONE_BINOP(GreaterOp)
  ROTATE_CLONE_BINOP(GreaterEqualOp)
  ROTATE_CLONE_BINOP(AddFOp)
  ROTATE_CLONE_BINOP(SubFOp)
  ROTATE_CLONE_BINOP(MulFOp)
  ROTATE_CLONE_BINOP(DivFOp)
  ROTATE_CLONE_BINOP(EqualFOp)
  ROTATE_CLONE_BINOP(NotEqualFOp)
  ROTATE_CLONE_BINOP(LessFOp)
  ROTATE_CLONE_BINOP(LessEqualFOp)
  ROTATE_CLONE_BINOP(GreaterFOp)
  ROTATE_CLONE_BINOP(GreaterEqualFOp)
#undef ROTATE_CLONE_BINOP

  if (clone == nullptr) {
    throw std::logic_error("LoopRotate: unclonable header op slipped past IsPureClonable");
  }
  (*value_map)[op->results_[0]] = res;
  for (size_t i = 0; i < clone->operands_.size(); ++i) {
    clone->operands_[i]->AddUse(clone, i);
  }
  return clone;
}

}  // namespace

auto LoopRotatePass::Run() -> void {
  // Opt-in (default OFF): SCEV, DeadLoopElim and StrengthReduce now handle the rotated IV/trip-count
  // shape, and LOOP_ROTATE=1 passes the directed and SysY functional suites. On the riscv dynamic
  // instruction benchmark it is still neutral overall with a few stable regressions from the extra
  // guard/control-flow structure, so keep it behind LOOP_ROTATE until the remaining profitability
  // gaps are closed.
  if (std::getenv("LOOP_ROTATE") == nullptr) {
    return;
  }
  PreRun();

  auto *cfg_pass = GET_PASS_CONTEXT()->GetSpecPass<analysis::CFGPass>("CFGPass");
  auto *loop_info = GET_PASS_CONTEXT()->GetSpecPass<analysis::LoopInfoPass>("LoopInfoPass");
  if (cfg_pass == nullptr || loop_info == nullptr) {
    throw std::logic_error("LoopRotatePass requires CFGPass and LoopInfoPass");
  }

  bool changed = false;
  for (auto *function_cfg : cfg_pass->Get()) {
    auto *mlir_fcfg = dynamic_cast<analysis::MLIRFunctionCFG *>(function_cfg);
    if (mlir_fcfg == nullptr) {
      continue;
    }
    auto *func_op = mlir_fcfg->GetFuncOp();
    // Innermost-first: loops come outermost-first, so iterate in reverse.
    const auto &loops = loop_info->GetLoops(function_cfg);
    for (auto it = loops.rbegin(); it != loops.rend(); ++it) {
      changed = TryRotate(func_op, *it) || changed;
    }
  }

  if (changed) {
    GET_PASS_CONTEXT()->InvalidateAnalysis();
  }
}

auto LoopRotatePass::TryRotate(mlir::base::FuncDefOp *func_op, analysis::Loop *loop) -> bool {
  auto *ctx = GET_MLIR_CONTEXT();
  auto *header = loop->GetHeader();
  if (header == nullptr || header->operations_.size() < 2) {
    return false;
  }
  auto *cond_br = DynamicOrNull<CondBrOp>(Terminator(header));
  if (cond_br == nullptr) {
    return false;
  }
  auto *true_target = cond_br->GetTrueTarget();
  auto *false_target = cond_br->GetFalseTarget();
  bool true_in = loop->Contains(true_target);
  bool false_in = loop->Contains(false_target);
  if (true_in == false_in) {
    return false;  // need exactly one in-loop and one exit successor
  }
  auto *body_target = true_in ? true_target : false_target;
  auto *exit_target = true_in ? false_target : true_target;

  // Dedicated preheader: header has exactly one predecessor outside the loop and
  // it jumps unconditionally to the header (guaranteed by LoopSimplify).
  Block *preheader = nullptr;
  bool has_latch = false;
  for (auto *pred : PredecessorsOf(func_op, header)) {
    if (loop->Contains(pred)) {
      has_latch = true;
      continue;
    }
    if (preheader != nullptr) {
      return false;  // more than one outside predecessor
    }
    preheader = pred;
  }
  if (preheader == nullptr || !has_latch) {
    return false;
  }
  if (DynamicOrNull<BrOp>(Terminator(preheader)) == nullptr) {
    return false;  // preheader must end in an unconditional branch
  }

  // Dedicated body entry: the body target is reached only from the header, so it
  // carries no phis that a new preheader edge would disturb.
  auto body_preds = PredecessorsOf(func_op, body_target);
  if (body_preds.size() != 1 || body_preds.front() != header) {
    return false;
  }

  // Single-exit: the header test is the loop's only way out. This makes every
  // out-of-loop use of a header value dominated by the exit, so SSA repair is a
  // clean two-way merge. Loops with `break` have extra exits and are skipped.
  for (auto *block : loop->GetBlocks()) {
    for (auto *succ : SuccessorsOf(block)) {
      bool succ_in = loop->Contains(succ);
      if (block == header) {
        if (succ != body_target && succ != exit_target) {
          return false;
        }
      } else if (!succ_in) {
        return false;
      }
    }
  }

  // The header must be phis followed by pure, clonable ops and the terminator.
  std::vector<PhiOp *> header_phis;
  std::vector<Operation *> header_ops;  // pure ops to clone into the guard
  bool seen_non_phi = false;
  for (size_t i = 0; i + 1 < header->operations_.size(); ++i) {
    auto *op = header->operations_[i];
    if (auto *phi = DynamicOrNull<PhiOp>(op)) {
      if (seen_non_phi) {
        return false;  // phis must lead the block
      }
      header_phis.emplace_back(phi);
      continue;
    }
    seen_non_phi = true;
    if (!IsPureClonable(op)) {
      return false;
    }
    header_ops.emplace_back(op);
  }

  // Seed the guard value map: each header phi maps to the value it receives from
  // the preheader edge (the value live on the very first iteration).
  std::unordered_map<Value *, Value *> value_map;
  for (auto *phi : header_phis) {
    Value *from_preheader = nullptr;
    const auto &blocks = phi->GetIncomingBlock();
    const auto &values = phi->GetIncomingValue();
    for (size_t i = 0; i < blocks.size(); ++i) {
      if (blocks[i] == preheader) {
        from_preheader = values[i];
        break;
      }
    }
    if (from_preheader == nullptr) {
      return false;  // header phi with no preheader incoming: not rotatable
    }
    value_map[phi->GetRes()] = from_preheader;
  }

  // ---- From here on the loop is committed to rotation; no more early exits. ----

  // 1. Clone the header's pure ops into the preheader as the guard computation.
  auto guard_insert = preheader->operations_.end() - 1;  // before preheader's br
  for (auto *op : header_ops) {
    auto *clone = CloneGuardOp(ctx, op, &value_map);
    guard_insert = preheader->operations_.insert(guard_insert, clone) + 1;
  }
  Value *original_cond = cond_br->GetCondition();
  Value *guard_cond = value_map.count(original_cond) != 0 ? value_map[original_cond] : original_cond;

  // 2. Build the rotated loop's dedicated blocks (LLVM shape):
  //    - rot_preheader: guard-true jumps here, here jumps into the body. Keeps a
  //      clean single-predecessor preheader for LICM.
  //    - loop_exit: the header test's exit edge lands here. Its only predecessor
  //      is the header, so it is a *dedicated* loop exit -- this is what lets
  //      Promotion sink stores out of the loop.
  //    - guard_merge: where the guard-skip path and the loop_exit path join and
  //      where the post-loop merge phis live.
  auto make_block = [&](const char *name, int base_id) -> Block * {
    auto *block = new Block();
    block->block_name_ = name;
    block->block_id_ = base_id + rot_block_counter++;
    return block;
  };
  auto *new_preheader = make_block("rot_preheader", 910000);
  auto *loop_exit = make_block("rot_loopexit", 920000);
  auto *guard_merge = make_block("rot_guardmerge", 930000);
  new_preheader->operations_.emplace_back(ctx->CreateOperation<BrOp>(body_target));
  loop_exit->operations_.emplace_back(ctx->CreateOperation<BrOp>(guard_merge));
  guard_merge->operations_.emplace_back(ctx->CreateOperation<BrOp>(exit_target));

  // 3. Route the header's exit edge through the dedicated loop_exit, and move
  //    exit_target phi incomings that came from the header onto the guard_merge
  //    edge (their values are repaired to the merge phi in step 7).
  if (cond_br->GetTrueTarget() == exit_target) {
    cond_br->SetTrueTarget(loop_exit);
  } else {
    cond_br->SetFalseTarget(loop_exit);
  }
  for (auto *op : exit_target->operations_) {
    if (auto *phi = DynamicOrNull<PhiOp>(op)) {
      auto values = phi->GetIncomingValue();
      auto blocks = phi->GetIncomingBlock();
      for (size_t i = 0; i < blocks.size(); ++i) {
        if (blocks[i] == header) {
          blocks[i] = guard_merge;
        }
      }
      phi->SetIncomings(values, blocks);
    }
  }

  // 4. Replace the preheader's unconditional branch with the guard: the body
  //    side goes to rot_preheader, the exit side to guard_merge.
  Block *guard_true = (cond_br->GetTrueTarget() == body_target) ? new_preheader : guard_merge;
  Block *guard_false = (cond_br->GetFalseTarget() == body_target) ? new_preheader : guard_merge;
  auto *guard = ctx->CreateOperation<CondBrOp>(guard_cond, guard_true, guard_false);
  guard_cond->AddUse(guard, 0);
  preheader->operations_.back() = guard;  // old br is orphaned (context-owned)

  // 5. Drop the preheader incoming from every header phi (that edge is gone).
  for (auto *phi : header_phis) {
    auto values = phi->GetIncomingValue();
    auto blocks = phi->GetIncomingBlock();
    for (auto *value : values) {
      value->RemoveUse(phi);
    }
    std::vector<Value *> kept_values;
    std::vector<Block *> kept_blocks;
    for (size_t i = 0; i < blocks.size(); ++i) {
      if (blocks[i] != preheader) {
        kept_values.emplace_back(values[i]);
        kept_blocks.emplace_back(blocks[i]);
      }
    }
    phi->SetIncomings(kept_values, kept_blocks);
    for (size_t i = 0; i < kept_values.size(); ++i) {
      kept_values[i]->AddUse(phi, i);
    }
  }

  // 6. Map every non-phi op to its block for use classification.
  std::unordered_map<Operation *, Block *> op_block;
  for (auto *block : func_op->GetBlocks()) {
    for (auto *op : block->operations_) {
      op_block[op] = block;
    }
  }
  for (auto *block : {new_preheader, loop_exit, guard_merge}) {
    for (auto *op : block->operations_) {
      op_block[op] = block;
    }
  }

  // 7. Reconstruct SSA for each header-defined value. A use inside the loop
  //    reads a merge phi in the body; a use after the loop reads a merge phi in
  //    guard_merge; a use still in the header keeps the original value.
  std::vector<Value *> header_values;
  for (auto *phi : header_phis) {
    header_values.emplace_back(phi->GetRes());
  }
  for (auto *op : header_ops) {
    header_values.emplace_back(op->results_[0]);
  }

  auto classify = [&](mlir::Use *use) -> int {
    Block *loc = nullptr;
    if (auto *phi = DynamicOrNull<PhiOp>(use->operation_)) {
      loc = phi->GetIncomingBlock()[use->operand_index_];
    } else {
      loc = op_block[use->operation_];
    }
    if (loc == header) {
      return 0;  // keep
    }
    return loop->Contains(loc) ? 1 : 2;  // 1 = body, 2 = exit
  };

  for (auto *v : header_values) {
    Value *v_clone = value_map[v];
    bool needs_body = false;
    bool needs_exit = false;
    for (auto *use : v->GetUses()) {
      int kind = classify(use);
      needs_body = needs_body || kind == 1;
      needs_exit = needs_exit || kind == 2;
    }

    // Create the (initially operand-less) merge phis so we have their results to
    // re-home uses onto; their incomings are wired afterwards so that the phi's
    // own reference to `v` is not itself re-homed.
    Value *body_merge = nullptr;
    PhiOp *body_phi = nullptr;
    Value *exit_merge = nullptr;
    PhiOp *exit_phi = nullptr;
    if (needs_body) {
      body_merge = ctx->CreateValue();
      body_merge->SetType(v->GetType());
      body_phi = ctx->CreateOperation<PhiOp>(body_merge);
      body_target->operations_.insert(body_target->operations_.begin(), body_phi);
    }
    if (needs_exit) {
      exit_merge = ctx->CreateValue();
      exit_merge->SetType(v->GetType());
      exit_phi = ctx->CreateOperation<PhiOp>(exit_merge);
      guard_merge->operations_.insert(guard_merge->operations_.begin(), exit_phi);
    }

    // Re-home uses: move Use objects off v onto the appropriate merge phi.
    std::vector<mlir::Use *> stay;
    for (auto *use : v->GetUses()) {
      int kind = classify(use);
      Value *target = kind == 1 ? body_merge : (kind == 2 ? exit_merge : v);
      if (target == v || target == nullptr) {
        stay.emplace_back(use);
        continue;
      }
      use->operation_->operands_[use->operand_index_] = target;
      target->GetUses().emplace_back(use);
    }
    v->GetUses() = stay;

    // Wire the merge phis. body_target's predecessors are {rot_preheader,
    // header}; guard_merge's are {preheader, loop_exit}.
    if (body_phi != nullptr) {
      body_phi->AddIncoming(v_clone, new_preheader);
      body_phi->AddIncoming(v, header);
      v_clone->AddUse(body_phi, 0);
      v->AddUse(body_phi, 1);
    }
    if (exit_phi != nullptr) {
      exit_phi->AddIncoming(v_clone, preheader);
      exit_phi->AddIncoming(v, loop_exit);
      v_clone->AddUse(exit_phi, 0);
      v->AddUse(exit_phi, 1);
    }
  }

  // 8. Rotation makes the old header a latch that now reads values defined in
  //    the body, so the block list must be re-linearized into reverse post-order
  //    (defs before uses) for the single-pass BASEIR->LLVM lowering.
  auto old_blocks = func_op->GetBlocks();
  old_blocks.emplace_back(new_preheader);
  old_blocks.emplace_back(loop_exit);
  old_blocks.emplace_back(guard_merge);
  std::unordered_set<Block *> known(old_blocks.begin(), old_blocks.end());

  std::vector<Block *> post_order;
  std::unordered_set<Block *> visited;
  std::vector<std::pair<Block *, size_t>> stack;
  Block *entry = old_blocks.front();
  stack.emplace_back(entry, 0);
  visited.insert(entry);
  while (!stack.empty()) {
    auto &[block, index] = stack.back();
    auto succs = SuccessorsOf(block);
    if (index < succs.size()) {
      Block *succ = succs[index++];
      if (known.count(succ) != 0 && visited.insert(succ).second) {
        stack.emplace_back(succ, 0);
      }
    } else {
      post_order.emplace_back(block);
      stack.pop_back();
    }
  }
  std::vector<Block *> reordered(post_order.rbegin(), post_order.rend());
  // Keep any unreachable blocks (defensive) so nothing is silently dropped.
  for (auto *block : old_blocks) {
    if (visited.count(block) == 0) {
      reordered.emplace_back(block);
    }
  }
  func_op->SetBlocks(std::move(reordered));
  return true;
}

}  // namespace middleend::pass::transform
