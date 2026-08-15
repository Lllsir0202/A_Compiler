#include "middleend/pass/analysis/memoryssa/memoryssa.h"
#include <stdexcept>
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/base/funccall.h"
#include "middleend/mlir/dialect/func/funccall.h"
#include "middleend/mlir/dialect/var/getvalue.h"
#include "middleend/mlir/dialect/var/set.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/block.h"
#include "middleend/mlir/utils/value/value.h"
#include "middleend/pass/analysis/alias/alias.h"
#include "middleend/pass/analysis/sideeffect/sideeffect.h"
#include "middleend/pass/analysis/cfg/cfg.h"
#include "middleend/pass/analysis/cfg/mlircfg.h"
#include "middleend/pass/analysis/domtree/domtree.h"

#define GET_PASS_CONTEXT() compiler_context_->GetPassContext()

namespace middleend::pass::analysis {
namespace {
auto IsStore(mlir::Operation *operation) -> bool {
  return mlir::DynamicOrNull<mlir::var::VarSetOp>(operation) != nullptr;
}
auto IsCall(mlir::Operation *operation) -> bool {
  return mlir::DynamicOrNull<mlir::base::FuncCallOp>(operation) != nullptr ||
         mlir::DynamicOrNull<mlir::func::FuncCallOp>(operation) != nullptr;
}
auto IsLoad(mlir::Operation *operation) -> bool {
  return mlir::DynamicOrNull<mlir::var::VarGetValOp>(operation) != nullptr;
}
auto CallMayWriteMemory(mlir::Operation *operation, AliasAnalysisPass *aa) -> bool {
  auto *se = aa != nullptr ? aa->GetSideEffect() : nullptr;
  if (se == nullptr) return true;  // conservative
  std::string callee;
  if (auto *bc = mlir::DynamicOrNull<mlir::base::FuncCallOp>(operation)) callee = bc->GetName();
  else if (auto *fc = mlir::DynamicOrNull<mlir::func::FuncCallOp>(operation)) callee = fc->GetName();
  if (callee.empty()) return true;
  if (!se->HasSummary(callee)) return true;
  const auto &s = se->GetSummary(callee);
  if (s.writes_all_globals || !s.written_globals.empty()) return true;
  // IsMemoryWriteNone also accounts for writes through a pointer argument
  // (getarray(a) being the common case), which name no global at all.
  return !s.IsMemoryWriteNone();
}

auto IsMemoryWrite(mlir::Operation *operation, AliasAnalysisPass *aa) -> bool {
  if (IsStore(operation)) return true;
  if (IsCall(operation)) return CallMayWriteMemory(operation, aa);
  return false;
}

auto BlockOf(CFGNode *node) -> mlir::Block * {
  auto *mlir_node = dynamic_cast<MLIRCFGNode *>(node);
  return mlir_node == nullptr ? nullptr : mlir_node->GetBlock();
}
}  // namespace

auto MemorySSAPass::Run() -> void {
  PreRun();
  ClearState();
  auto *cfg_pass = GET_PASS_CONTEXT()->GetSpecPass<CFGPass>("CFGPass");
  auto *dom_tree_pass = GET_PASS_CONTEXT()->GetSpecPass<DomTreePass>("DomTreePass");
  alias_analysis_ = GET_PASS_CONTEXT()->GetSpecPass<AliasAnalysisPass>("AliasAnalysisPass");
  if (cfg_pass == nullptr || dom_tree_pass == nullptr || alias_analysis_ == nullptr) {
    throw std::logic_error("MemorySSAPass requires CFGPass, DomTreePass and AliasAnalysisPass");
  }

  for (auto *function_cfg : cfg_pass->Get()) {
    auto *mlir_function_cfg = dynamic_cast<MLIRFunctionCFG *>(function_cfg);
    if (mlir_function_cfg == nullptr) {
      throw std::logic_error("MemorySSAPass only supports MLIRFunctionCFG");
    }
    auto *dom_tree = dom_tree_pass->GetDomTree(function_cfg);
    if (dom_tree == nullptr) {
      throw std::logic_error("MemorySSAPass requires the dominator tree of every function");
    }
    RunOnFunction(mlir_function_cfg, dom_tree);
  }
  Validate();
}

auto MemorySSAPass::RunOnFunction(MLIRFunctionCFG *function_cfg, DomTree *dom_tree) -> void {
  dominance_frontier_.clear();
  ComputeDominanceFrontier(function_cfg, dom_tree);
  PlacePhis(function_cfg);

  auto *entry = dom_tree->GetEntryNode();
  if (entry == nullptr) {
    return;
  }
  auto *live_on_entry = new MemoryDef(BlockOf(entry->GetCFGNode()), nullptr, nullptr);
  owned_accesses_.emplace_back(live_on_entry);
  Rename(entry, live_on_entry);
}

auto MemorySSAPass::Idom(DomTree *dom_tree, CFGNode *node) -> CFGNode * {
  auto *dom_node = dom_tree->GetDomTreeNode(node);
  if (dom_node == nullptr || dom_node->GetIDom() == nullptr) {
    return nullptr;
  }
  return dom_node->GetIDom()->GetCFGNode();
}

auto MemorySSAPass::ComputeDominanceFrontier(MLIRFunctionCFG *function_cfg, DomTree *dom_tree) -> void {
  for (auto *node : function_cfg->GetNodes()) {
    if (dom_tree->GetDomTreeNode(node) == nullptr) {
      continue;  // unreachable block, not part of the dominator tree
    }
    auto predecessors = node->GetPredecessors();
    if (predecessors.size() < 2) {
      continue;
    }
    auto *idom = Idom(dom_tree, node);
    for (auto *predecessor : predecessors) {
      auto *runner = predecessor;
      while (runner != nullptr && runner != idom) {
        dominance_frontier_[runner].insert(node);
        runner = Idom(dom_tree, runner);
      }
    }
  }
}

auto MemorySSAPass::PlacePhis(MLIRFunctionCFG *function_cfg) -> void {
  std::vector<CFGNode *> worklist;
  std::unordered_set<CFGNode *> has_phi;
  for (auto *node : function_cfg->GetNodes()) {
    auto *block = BlockOf(node);
    if (block == nullptr) {
      continue;
    }
    for (auto *operation : block->operations_) {
      if (IsMemoryWrite(operation, alias_analysis_)) {
        worklist.emplace_back(node);
        break;
      }
    }
  }

  // Iterated dominance frontier: a phi at each node in the closure of DF over the def nodes.
  while (!worklist.empty()) {
    auto *node = worklist.back();
    worklist.pop_back();
    auto frontier = dominance_frontier_.find(node);
    if (frontier == dominance_frontier_.end()) {
      continue;
    }
    for (auto *frontier_node : frontier->second) {
      if (has_phi.count(frontier_node) > 0) {
        continue;
      }
      has_phi.insert(frontier_node);
      auto *block = BlockOf(frontier_node);
      auto *phi = new MemoryPhi(block);
      owned_accesses_.emplace_back(phi);
      block_to_phi_[block] = phi;
      worklist.emplace_back(frontier_node);
    }
  }
}

auto MemorySSAPass::Rename(DomTreeNode *dom_node, MemoryAccess *incoming) -> void {
  auto *cfg_node = dom_node->GetCFGNode();
  auto *block = BlockOf(cfg_node);
  if (block == nullptr) {
    return;
  }

  MemoryAccess *current = incoming;
  auto phi_iter = block_to_phi_.find(block);
  if (phi_iter != block_to_phi_.end()) {
    current = phi_iter->second;
  }

  for (auto *operation : block->operations_) {
    if (IsLoad(operation)) {
      auto *use = new MemoryUse(block, operation, current);
      owned_accesses_.emplace_back(use);
      op_to_access_[operation] = use;
    } else if (IsMemoryWrite(operation, alias_analysis_)) {
      auto *def = new MemoryDef(block, operation, current);
      owned_accesses_.emplace_back(def);
      op_to_access_[operation] = def;
      current = def;
    }
  }

  // Feed the reaching definition at the end of this block into the phis of CFG successors.
  for (auto *successor : cfg_node->GetSuccessors()) {
    auto *successor_block = BlockOf(successor);
    auto successor_phi = block_to_phi_.find(successor_block);
    if (successor_phi != block_to_phi_.end()) {
      successor_phi->second->AddIncoming(block, current);
    }
  }

  for (auto *child : dom_node->GetChildren()) {
    Rename(child, current);
  }
}

auto MemorySSAPass::GetMemoryAccess(mlir::Operation *operation) -> MemoryAccess * {
  auto iter = op_to_access_.find(operation);
  return iter == op_to_access_.end() ? nullptr : iter->second;
}

auto MemorySSAPass::GetClobberingAccess(mlir::Operation *load) -> MemoryAccess * {
  auto *access = GetMemoryAccess(load);
  if (access == nullptr || access->GetKind() != MemoryAccess::Kind::Use) {
    return nullptr;
  }
  auto *load_op = mlir::DynamicOrNull<mlir::var::VarGetValOp>(load);
  if (load_op == nullptr) {
    return nullptr;
  }
  std::unordered_set<MemoryAccess *> visited;
  return WalkClobber(static_cast<MemoryUse *>(access)->GetDefiningAccess(), load_op->GetPtr(), &visited);
}

auto MemorySSAPass::WalkClobber(MemoryAccess *access, mlir::Value *pointer,
                                std::unordered_set<MemoryAccess *> *visited) -> MemoryAccess * {
  while (access != nullptr) {
    if (access->GetKind() == MemoryAccess::Kind::Def) {
      auto *def = static_cast<MemoryDef *>(access);
      if (DefClobbers(def, pointer)) {
        return access;
      }
      access = def->GetDefiningAccess();
      continue;
    }
    // Phi: look through it when every incoming path resolves to the same clobber.
    if (visited->count(access) > 0) {
      return access;
    }
    visited->insert(access);
    auto *phi = static_cast<MemoryPhi *>(access);
    MemoryAccess *common = nullptr;
    bool first = true;
    for (const auto &incoming : phi->GetIncomings()) {
      auto *resolved = WalkClobber(incoming.second, pointer, visited);
      if (first) {
        common = resolved;
        first = false;
      } else if (resolved != common) {
        return access;
      }
    }
    return first ? access : common;
  }
  return access;
}

auto MemorySSAPass::DefClobbers(MemoryDef *def, mlir::Value *pointer) -> bool {
  auto *op = def->GetOperation();
  if (op == nullptr) {
    return true;  // live-on-entry establishes the initial memory value: stop here.
  }
  if (auto *store_op = mlir::DynamicOrNull<mlir::var::VarSetOp>(op)) {
    return alias_analysis_->Alias(pointer, store_op->GetPtr()) != AliasResult::NoAlias;
  }
  return alias_analysis_->CallModRef(op, pointer);
}

auto MemorySSAPass::ClearState() -> void {
  for (auto *access : owned_accesses_) {
    delete access;
  }
  owned_accesses_.clear();
  op_to_access_.clear();
  block_to_phi_.clear();
  dominance_frontier_.clear();
}

}  // namespace middleend::pass::analysis
