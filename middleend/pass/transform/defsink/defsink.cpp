#include "middleend/pass/transform/defsink/defsink.h"
#include <stdexcept>
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/base/phi.h"
#include "middleend/mlir/dialect/var/def.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/block.h"
#include "middleend/mlir/utils/value/value.h"
#include "middleend/pass/analysis/cfg/cfg.h"
#include "middleend/pass/analysis/cfg/mlircfg.h"
#include "middleend/pass/analysis/domtree/domtree.h"
#include "middleend/pass/analysis/loopinfo/loopinfo.h"

#define GET_PASS_CONTEXT() compiler_context_->GetPassContext()

namespace middleend::pass::transform {

auto VarDefSinkPass::Run() -> void {
  PreRun();
  auto *cfg_pass = GET_PASS_CONTEXT()->GetSpecPass<analysis::CFGPass>("CFGPass");
  dom_tree_pass_ = GET_PASS_CONTEXT()->GetSpecPass<analysis::DomTreePass>("DomTreePass");
  loop_info_pass_ = GET_PASS_CONTEXT()->GetSpecPass<analysis::LoopInfoPass>("LoopInfoPass");
  if (cfg_pass == nullptr || dom_tree_pass_ == nullptr || loop_info_pass_ == nullptr) {
    throw std::logic_error("VarDefSinkPass requires CFGPass, DomTreePass and LoopInfoPass");
  }

  bool changed = false;
  for (auto *function_cfg : cfg_pass->Get()) {
    auto *mlir_function_cfg = dynamic_cast<analysis::MLIRFunctionCFG *>(function_cfg);
    if (mlir_function_cfg == nullptr) {
      throw std::logic_error("VarDefSinkPass only supports MLIRFunctionCFG");
    }
    changed = RunOnFunction(mlir_function_cfg) || changed;
  }

  if (changed) {
    GET_PASS_CONTEXT()->InvalidateAnalysis();
  }
}

auto VarDefSinkPass::FindSinkTarget(analysis::MLIRFunctionCFG *function_cfg, mlir::var::VarDefOp *def_op,
                                    mlir::Block *from) -> mlir::Block * {
  auto *dom_tree = dom_tree_pass_->GetDomTree(function_cfg);
  if (dom_tree == nullptr) {
    return from;
  }
  auto *from_node = function_cfg->GetNodeByBlock(from);
  if (from_node == nullptr) {
    return from;
  }
  auto *from_dom = dom_tree->GetDomTreeNode(from_node);
  if (from_dom == nullptr) {
    return from;
  }
  // Sinking into a loop the definition is currently outside of would re-run the initialization on
  // every iteration, wiping what earlier iterations wrote.
  auto *from_loop = loop_info_pass_->GetLoopFor(from);

  // Every block holding a use, so a candidate can be checked for dominating all of them.
  std::vector<mlir::Block *> use_blocks;
  for (auto *use : def_op->GetRes()->GetUses()) {
    if (use->operation_ == nullptr) {
      continue;
    }
    mlir::Block *use_block = nullptr;
    for (auto *cfg_node : function_cfg->GetNodes()) {
      auto *mlir_node = dynamic_cast<analysis::MLIRCFGNode *>(cfg_node);
      if (mlir_node == nullptr) {
        continue;
      }
      for (auto *operation : mlir_node->GetBlock()->operations_) {
        if (operation == use->operation_) {
          use_block = mlir_node->GetBlock();
          break;
        }
      }
      if (use_block != nullptr) {
        break;
      }
    }
    if (use_block == nullptr) {
      return from;  // a use we cannot place: stay put.
    }
    use_blocks.emplace_back(use_block);
  }

  // Walk down the dominator tree while exactly one child still dominates every use. A block that
  // dominates all uses is a legal home for the definition; the deepest such block is the one whose
  // predecessors' guards have already been passed.
  mlir::Block *best = from;
  auto *current = from_dom;
  while (true) {
    mlir::Block *next = nullptr;
    for (auto *child : current->GetChildren()) {
      auto *child_cfg = dynamic_cast<analysis::MLIRCFGNode *>(child->GetCFGNode());
      if (child_cfg == nullptr) {
        continue;
      }
      auto *child_block = child_cfg->GetBlock();
      bool dominates_all = true;
      for (auto *use_block : use_blocks) {
        auto *use_node = function_cfg->GetNodeByBlock(use_block);
        if (use_node == nullptr || !dom_tree->Dominates(child_cfg, use_node)) {
          dominates_all = false;
          break;
        }
      }
      if (!dominates_all) {
        continue;
      }
      // Never sink into a deeper loop nest than the definition started in.
      auto *child_loop = loop_info_pass_->GetLoopFor(child_block);
      if (child_loop != from_loop) {
        continue;
      }
      if (next != nullptr) {
        // Two children each dominating every use is impossible in a well-formed dominator tree, but
        // bail out rather than pick arbitrarily.
        next = nullptr;
        break;
      }
      next = child_block;
      current = child;
    }
    if (next == nullptr) {
      break;
    }
    best = next;
  }
  return best;
}

auto VarDefSinkPass::RunOnFunction(analysis::MLIRFunctionCFG *function_cfg) -> bool {
  bool changed = false;
  // Collect first: the walk below moves operations between blocks.
  std::vector<std::pair<mlir::Block *, mlir::var::VarDefOp *>> candidates;
  for (auto *cfg_node : function_cfg->GetNodes()) {
    auto *mlir_node = dynamic_cast<analysis::MLIRCFGNode *>(cfg_node);
    if (mlir_node == nullptr) {
      continue;
    }
    for (auto *operation : mlir_node->GetBlock()->operations_) {
      if (auto *def_op = mlir::DynamicOrNull<mlir::var::VarDefOp>(operation)) {
        // A scalar's definition lowers to nothing but the stack slot, so there is nothing to save.
        if (def_op->GetDimensions().empty()) {
          continue;
        }
        candidates.emplace_back(mlir_node->GetBlock(), def_op);
      }
    }
  }

  for (auto &[block, def_op] : candidates) {
    auto *target = FindSinkTarget(function_cfg, def_op, block);
    if (target == block) {
      continue;
    }
    // The initializer operands must already be available at the target. They are constants or
    // entry-block values in practice; anything else would need its own dominance check.
    bool operands_available = true;
    for (auto *init : def_op->GetInits()) {
      if (init != nullptr && init->op_ != nullptr) {
        operands_available = false;
        break;
      }
    }
    if (!operands_available) {
      continue;
    }
    block->RemoveOperation(def_op);
    // As early in the target block as possible, so a use in that very block still sees the
    // initialized memory -- but after any leading phis, which must stay at the top of the block.
    size_t insert_at = 0;
    while (insert_at < target->operations_.size() &&
           mlir::DynamicOrNull<mlir::base::PhiOp>(target->operations_[insert_at]) != nullptr) {
      ++insert_at;
    }
    target->operations_.insert(target->operations_.begin() + static_cast<long>(insert_at), def_op);
    ++sunk_count_;
    changed = true;
  }
  return changed;
}

}  // namespace middleend::pass::transform
