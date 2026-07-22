#include "middleend/pass/transform/gvnpre/gvnpre.h"
#include <utility>
#include <vector>
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/base/binaryop.h"
#include "middleend/mlir/dialect/base/cast.h"
#include "middleend/mlir/dialect/base/phi.h"
#include "middleend/mlir/dialect/base/unaryop.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/block.h"
#include "middleend/mlir/utils/value/value.h"
#include "middleend/pass/analysis/cfg/cfg.h"
#include "middleend/pass/analysis/cfg/mlircfg.h"
#include "middleend/pass/analysis/domtree/domtree.h"

#define GET_PASS_CONTEXT() compiler_context_->GetPassContext()
#define GET_MLIR_CONTEXT() compiler_context_->GetIRContext()->GetMLIRContext()

namespace middleend::pass::transform {
namespace {
template <typename OperationType>
auto Is(mlir::Operation *operation) -> bool {
  return mlir::DynamicOrNull<OperationType>(operation) != nullptr;
}
}  // namespace

auto GVNPREPass::Run() -> void {
  PreRun();
  auto *cfg_pass = GET_PASS_CONTEXT()->GetSpecPass<analysis::CFGPass>("CFGPass");
  auto *dom_tree_pass = GET_PASS_CONTEXT()->GetSpecPass<analysis::DomTreePass>("DomTreePass");
  if (cfg_pass == nullptr || dom_tree_pass == nullptr) {
    throw std::logic_error("GVNPREPass requires CFGPass and DomTreePass");
  }

  bool changed = false;
  for (auto *function_cfg : cfg_pass->Get()) {
    auto *mlir_function_cfg = dynamic_cast<analysis::MLIRFunctionCFG *>(function_cfg);
    if (mlir_function_cfg == nullptr) {
      throw std::logic_error("GVNPREPass only supports MLIRFunctionCFG");
    }
    auto *dom_tree = dom_tree_pass->GetDomTree(function_cfg);
    if (dom_tree == nullptr) {
      throw std::logic_error("GVNPREPass requires the dominator tree of every function");
    }
    changed = RunOnFunction(mlir_function_cfg, dom_tree) || changed;
  }
  if (changed) {
    GET_PASS_CONTEXT()->InvalidateAnalysis();
  }
}

auto GVNPREPass::RunOnFunction(analysis::MLIRFunctionCFG *function_cfg, analysis::DomTree *dom_tree) -> bool {
  changed_ = false;
  op_to_block_.clear();
  avail_out_.clear();
  inserted_.clear();
  dead_values_.clear();

  BuildOpToBlock(function_cfg);
  auto *entry = dom_tree->GetEntryNode();
  if (entry == nullptr) {
    return false;
  }
  BuildAvailOut(entry, {});

  // Insert at every join point (a block with several predecessors).
  for (auto *cfg_node : function_cfg->GetNodes()) {
    auto *mlir_node = dynamic_cast<analysis::MLIRCFGNode *>(cfg_node);
    if (mlir_node == nullptr || mlir_node->GetPredecessors().size() < 2) {
      continue;
    }
    TryPRE(mlir_node->GetBlock(), function_cfg, dom_tree);
  }
  return changed_;
}

void GVNPREPass::BuildOpToBlock(analysis::MLIRFunctionCFG *function_cfg) {
  for (auto *cfg_node : function_cfg->GetNodes()) {
    auto *mlir_node = dynamic_cast<analysis::MLIRCFGNode *>(cfg_node);
    if (mlir_node == nullptr) {
      continue;
    }
    for (auto *operation : mlir_node->GetBlock()->operations_) {
      op_to_block_[operation] = mlir_node->GetBlock();
    }
  }
}

void GVNPREPass::BuildAvailOut(analysis::DomTreeNode *node,
                               const std::unordered_map<Expr, mlir::Value *, ExprHash> &parent_avail) {
  auto *mlir_node = dynamic_cast<analysis::MLIRCFGNode *>(node->GetCFGNode());
  if (mlir_node == nullptr) {
    return;
  }
  auto *block = mlir_node->GetBlock();
  auto avail = parent_avail;  // inherit availability from the immediate dominator
  for (auto *operation : block->operations_) {
    Expr key;
    if (ComputeKey(operation, &key) && avail.find(key) == avail.end()) {
      avail.emplace(key, operation->results_[0]);
    }
  }
  avail_out_[block] = avail;
  for (auto *child : node->GetChildren()) {
    BuildAvailOut(child, avail);
  }
}

auto GVNPREPass::BlockOf(mlir::Value *value) -> mlir::Block * {
  if (value == nullptr || value->op_ == nullptr) {
    return nullptr;  // parameter / global: available everywhere
  }
  auto it = op_to_block_.find(value->op_);
  return it == op_to_block_.end() ? nullptr : it->second;
}

auto GVNPREPass::AvailableAt(mlir::Value *value, mlir::Block *where, analysis::MLIRFunctionCFG *cfg,
                             analysis::DomTree *dom) -> bool {
  auto *def_block = BlockOf(value);
  if (def_block == nullptr) {
    return true;  // defined outside any block (param/global)
  }
  auto *def_node = cfg->GetNodeByBlock(def_block);
  auto *where_node = cfg->GetNodeByBlock(where);
  if (def_node == nullptr || where_node == nullptr) {
    return false;
  }
  return dom->Dominates(def_node, where_node);
}

auto GVNPREPass::TryPRE(mlir::Block *merge, analysis::MLIRFunctionCFG *function_cfg, analysis::DomTree *dom_tree)
    -> bool {
  auto *merge_node = function_cfg->GetNodeByBlock(merge);
  std::vector<mlir::Block *> preds;
  for (auto *p : merge_node->GetPredecessors()) {
    auto *mp = dynamic_cast<analysis::MLIRCFGNode *>(p);
    if (mp == nullptr) {
      return false;
    }
    preds.emplace_back(mp->GetBlock());
  }
  if (preds.size() < 2) {
    return false;
  }

  // The incoming value an operand contributes on the edge pred -> merge: the phi's
  // incoming value if the operand is a phi defined in `merge`, otherwise the value
  // itself (it must dominate the predecessor, checked separately). Returns nullptr
  // if the operand is a merge phi with no entry for this predecessor.
  auto translate = [&](mlir::Value *operand, mlir::Block *pred, bool *ok) -> mlir::Value * {
    *ok = true;
    if (operand != nullptr && operand->op_ != nullptr && Is<mlir::base::PhiOp>(operand->op_) &&
        BlockOf(operand) == merge) {
      auto *phi = mlir::DynamicOrNull<mlir::base::PhiOp>(operand->op_);
      const auto &values = phi->GetIncomingValue();
      const auto &blocks = phi->GetIncomingBlock();
      for (size_t i = 0; i < blocks.size(); ++i) {
        if (blocks[i] == pred) {
          return values[i];
        }
      }
      *ok = false;
      return nullptr;
    }
    return operand;
  };

  auto make_key = [&](mlir::Operation *op, const std::vector<mlir::Value *> &ops) -> Expr {
    Expr k;
    ComputeKey(op, &k);  // fills opcode / immediate flags
    k.operands = ops;
    if (IsCommutative(op) && k.operands.size() == 2 && k.operands[0] > k.operands[1]) {
      std::swap(k.operands[0], k.operands[1]);
    }
    return k;
  };

  std::vector<mlir::Operation *> snapshot = merge->operations_;
  std::vector<mlir::Operation *> new_phis;
  std::unordered_map<mlir::Operation *, bool> removed;

  for (auto *op : snapshot) {
    if (inserted_.count(op) != 0) {
      continue;
    }
    Expr key;
    if (!ComputeKey(op, &key)) {
      continue;  // not a pure scalar expression
    }
    const auto &operands = op->operands_;

    // For each predecessor, translate the expression onto the edge and look it up in
    // that predecessor's availability set.
    std::vector<mlir::Value *> leaders(preds.size(), nullptr);
    std::vector<bool> missing(preds.size(), false);
    std::vector<std::vector<mlir::Value *>> translated(preds.size());
    bool feasible = true;
    for (size_t pi = 0; pi < preds.size() && feasible; ++pi) {
      auto *pred = preds[pi];
      std::vector<mlir::Value *> tops;
      tops.reserve(operands.size());
      for (auto *operand : operands) {
        bool ok = true;
        auto *t = translate(operand, pred, &ok);
        if (!ok || !AvailableAt(t, pred, function_cfg, dom_tree)) {
          feasible = false;
          break;
        }
        tops.emplace_back(t);
      }
      if (!feasible) {
        break;
      }
      translated[pi] = tops;
      Expr kp = make_key(op, tops);
      auto it = avail_out_[pred].find(kp);
      bool usable_leader =
          it != avail_out_[pred].end() && BlockOf(it->second) != merge && dead_values_.count(it->second) == 0;
      if (usable_leader) {
        // A leader defined in `merge` itself (reachable because merge dominates a latch
        // predecessor) cannot feed a phi at the top of merge, and a leader whose op this
        // pass already removed is stale -- either makes the candidate infeasible.
        leaders[pi] = it->second;
      } else if (it != avail_out_[pred].end()) {
        feasible = false;
      } else {
        missing[pi] = true;
        // Only hoist into a predecessor that flows solely to `merge`; hoisting across
        // a critical edge would speculate the computation onto a sibling path.
        auto *pred_node = function_cfg->GetNodeByBlock(pred);
        if (pred_node == nullptr || pred_node->GetSuccessors().size() != 1) {
          feasible = false;
        }
      }
    }
    if (!feasible) {
      continue;
    }
    size_t avail_count = 0;
    for (auto *l : leaders) {
      if (l != nullptr) {
        avail_count++;
      }
    }
    if (avail_count == 0) {
      continue;  // fully absent: this is speculation / LICM territory, not PRE
    }

    // Materialize the hoisted computation in each predecessor that lacks it.
    for (size_t pi = 0; pi < preds.size(); ++pi) {
      if (!missing[pi]) {
        continue;
      }
      auto *pred = preds[pi];
      auto *res = GET_MLIR_CONTEXT()->CreateValue();
      res->SetType(op->results_[0]->GetType());
      auto *new_op = MakeOp(op, res, translated[pi]);
      if (new_op == nullptr) {
        feasible = false;
        break;
      }
      for (size_t oi = 0; oi < translated[pi].size(); ++oi) {
        translated[pi][oi]->AddUse(new_op, oi);
      }
      pred->operations_.insert(pred->operations_.end() - 1, new_op);  // before the terminator
      op_to_block_[new_op] = pred;
      inserted_[new_op] = true;
      avail_out_[pred].emplace(make_key(op, translated[pi]), res);
      leaders[pi] = res;
    }
    if (!feasible) {
      continue;
    }

    // Merge the per-predecessor values and replace the redundant computation.
    auto *phi_res = GET_MLIR_CONTEXT()->CreateValue();
    phi_res->SetType(op->results_[0]->GetType());
    auto *phi = mlir::base::PhiOp::Create(phi_res);
    for (size_t pi = 0; pi < preds.size(); ++pi) {
      phi->AddIncoming(leaders[pi], preds[pi]);
      leaders[pi]->AddUse(phi, pi);
    }
    op_to_block_[phi] = merge;
    inserted_[phi] = true;
    new_phis.emplace_back(phi);

    op->results_[0]->ReplaceAllUsesWith(phi_res);
    for (auto *operand : op->operands_) {
      if (operand != nullptr) {
        operand->RemoveUse(op);
      }
    }
    dead_values_[op->results_[0]] = true;  // stale in avail_out from here on
    removed[op] = true;
    changed_ = true;
  }

  if (new_phis.empty() && removed.empty()) {
    return false;
  }
  // Rebuild the block: all phis first (existing kept + newly inserted), then the rest.
  std::vector<mlir::Operation *> rebuilt;
  rebuilt.reserve(merge->operations_.size() + new_phis.size());
  for (auto *op : merge->operations_) {
    if (removed.count(op) == 0 && Is<mlir::base::PhiOp>(op)) {
      rebuilt.emplace_back(op);
    }
  }
  for (auto *phi : new_phis) {
    rebuilt.emplace_back(phi);
  }
  for (auto *op : merge->operations_) {
    if (removed.count(op) == 0 && !Is<mlir::base::PhiOp>(op)) {
      rebuilt.emplace_back(op);
    }
  }
  merge->operations_ = std::move(rebuilt);
  return true;
}

auto GVNPREPass::ComputeKey(mlir::Operation *operation, Expr *key) -> bool {
  if (!IsPurePRE(operation) || operation->results_.size() != 1) {
    return false;
  }
  key->opcode = &typeid(*operation);
  key->operands = operation->operands_;
  if (IsCommutative(operation) && key->operands.size() == 2 && key->operands[0] > key->operands[1]) {
    std::swap(key->operands[0], key->operands[1]);
  }
  return true;
}

auto GVNPREPass::IsPurePRE(mlir::Operation *operation) -> bool {
  // Pure, fixed-arity scalar expressions only (no gep / loads / calls / consts): those
  // are the ones MakeOp can rematerialize in a predecessor.
  if (Is<mlir::base::AddOp>(operation) || Is<mlir::base::SubOp>(operation) || Is<mlir::base::MulOp>(operation) ||
      Is<mlir::base::DivOp>(operation) || Is<mlir::base::ModOp>(operation) || Is<mlir::base::EqualOp>(operation) ||
      Is<mlir::base::NotEqualOp>(operation) || Is<mlir::base::LessOp>(operation) ||
      Is<mlir::base::LessEqualOp>(operation) || Is<mlir::base::GreaterOp>(operation) ||
      Is<mlir::base::GreaterEqualOp>(operation)) {
    return true;
  }
  if (Is<mlir::base::AddFOp>(operation) || Is<mlir::base::SubFOp>(operation) || Is<mlir::base::MulFOp>(operation) ||
      Is<mlir::base::DivFOp>(operation) || Is<mlir::base::EqualFOp>(operation) ||
      Is<mlir::base::NotEqualFOp>(operation) || Is<mlir::base::LessFOp>(operation) ||
      Is<mlir::base::LessEqualFOp>(operation) || Is<mlir::base::GreaterFOp>(operation) ||
      Is<mlir::base::GreaterEqualFOp>(operation)) {
    return true;
  }
  if (Is<mlir::base::NegOp>(operation) || Is<mlir::base::NegFOp>(operation) || Is<mlir::base::NotOp>(operation)) {
    return true;
  }
  if (Is<mlir::base::Int2FloatOp>(operation) || Is<mlir::base::Float2IntOp>(operation) ||
      Is<mlir::base::Bool2IntOp>(operation) || Is<mlir::base::Int2BoolOp>(operation) ||
      Is<mlir::base::Bool2FloatOp>(operation) || Is<mlir::base::Float2BoolOp>(operation)) {
    return true;
  }
  return false;
}

auto GVNPREPass::IsCommutative(mlir::Operation *operation) -> bool {
  return Is<mlir::base::AddOp>(operation) || Is<mlir::base::MulOp>(operation) || Is<mlir::base::AddFOp>(operation) ||
         Is<mlir::base::MulFOp>(operation) || Is<mlir::base::EqualOp>(operation) ||
         Is<mlir::base::NotEqualOp>(operation) || Is<mlir::base::EqualFOp>(operation) ||
         Is<mlir::base::NotEqualFOp>(operation);
}

auto GVNPREPass::MakeOp(mlir::Operation *sample, mlir::Value *res, const std::vector<mlir::Value *> &operands)
    -> mlir::Operation * {
  auto *ctx = GET_MLIR_CONTEXT();
#define PRE_BINARY(T)                                                          \
  if (Is<mlir::base::T>(sample)) {                                             \
    if (operands.size() != 2) {                                                \
      return nullptr;                                                          \
    }                                                                          \
    return ctx->CreateOperation<mlir::base::T>(res, operands[0], operands[1]); \
  }
#define PRE_UNARY(T)                                              \
  if (Is<mlir::base::T>(sample)) {                                \
    if (operands.size() != 1) {                                   \
      return nullptr;                                             \
    }                                                             \
    return ctx->CreateOperation<mlir::base::T>(res, operands[0]); \
  }
  PRE_BINARY(AddOp)
  PRE_BINARY(SubOp)
  PRE_BINARY(MulOp)
  PRE_BINARY(DivOp)
  PRE_BINARY(ModOp)
  PRE_BINARY(EqualOp)
  PRE_BINARY(NotEqualOp)
  PRE_BINARY(LessOp)
  PRE_BINARY(LessEqualOp)
  PRE_BINARY(GreaterOp)
  PRE_BINARY(GreaterEqualOp)
  PRE_BINARY(AddFOp)
  PRE_BINARY(SubFOp)
  PRE_BINARY(MulFOp)
  PRE_BINARY(DivFOp)
  PRE_BINARY(EqualFOp)
  PRE_BINARY(NotEqualFOp)
  PRE_BINARY(LessFOp)
  PRE_BINARY(LessEqualFOp)
  PRE_BINARY(GreaterFOp)
  PRE_BINARY(GreaterEqualFOp)
  PRE_UNARY(NegOp)
  PRE_UNARY(NegFOp)
  PRE_UNARY(NotOp)
  PRE_UNARY(Int2FloatOp)
  PRE_UNARY(Float2IntOp)
  PRE_UNARY(Bool2IntOp)
  PRE_UNARY(Int2BoolOp)
  PRE_UNARY(Bool2FloatOp)
  PRE_UNARY(Float2BoolOp)
#undef PRE_BINARY
#undef PRE_UNARY
  return nullptr;
}

}  // namespace middleend::pass::transform
