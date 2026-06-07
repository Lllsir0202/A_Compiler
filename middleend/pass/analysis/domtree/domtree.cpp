#include "middleend/pass/analysis/domtree/domtree.h"
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/base/funcdef.h"
#include "middleend/pass/analysis/cfg/cfg.h"
#include "middleend/pass/analysis/cfg/mlircfg.h"
#define GET_PASS_CONTEXT() (compiler_context_->GetPassContext())
namespace middleend::pass::analysis {
namespace {
auto GetFunctionDebugName(FunctionCFG *function_cfg) -> std::string {
  if (auto *mlir_function_cfg = dynamic_cast<MLIRFunctionCFG *>(function_cfg)) {
    return mlir_function_cfg->GetFuncOp()->GetName();
  }
  return "unknown";
}

auto GetLabelDebugName(FunctionCFG *function_cfg, CFGNode *cfg_node) -> std::string {
  if (auto *mlir_function_cfg = dynamic_cast<MLIRFunctionCFG *>(function_cfg)) {
    if (auto *mlir_cfg_node = dynamic_cast<MLIRCFGNode *>(cfg_node)) {
      auto blocks = mlir_function_cfg->GetFuncOp()->GetBlocks();
      auto *block = mlir_cfg_node->GetBlock();
      for (size_t i = 0; i < blocks.size(); ++i) {
        if (blocks[i] == block) {
          return "label_" + std::to_string(i);
        }
      }
    }
  }
  return "cfg_" + std::to_string(cfg_node->GetId());
}
}  // namespace

// Normal and Post DomTree implementation
auto NormalDomTreeHelper::GetPredecessors(CFGNode *node) -> std::vector<CFGNode *> { return node->GetPredecessors(); }
auto NormalDomTreeHelper::GetSuccessors(CFGNode *node) -> std::vector<CFGNode *> { return node->GetSuccessors(); }
auto NormalDomTreeHelper::GetEntryNode(FunctionCFG *function_cfg) -> CFGNode * { return function_cfg->GetEntryNode(); }

auto PostDomTreeHelper::GetPredecessors(CFGNode *node) -> std::vector<CFGNode *> { return node->GetSuccessors(); }
auto PostDomTreeHelper::GetSuccessors(CFGNode *node) -> std::vector<CFGNode *> { return node->GetPredecessors(); }
auto PostDomTreeHelper::GetEntryNode(FunctionCFG *function_cfg) -> CFGNode * {
  // We make sure that it will be runned after the unify pass
  // So the exit nodes will be unified to one exit node, which will be the entry node for the post-dominator tree
  auto exit_nodes = function_cfg->GetExitNodes();
  if (exit_nodes.empty()) {
    return nullptr;
  }
  if (exit_nodes.size() > 1) {
    return nullptr;
  }
  return exit_nodes[0];
}

// Start from the DomTreeNode
auto DomTreeNode::Link() -> void { ancestor_ = parent_; }

auto DomTreeNode::Eval() -> DomTreeNode * {
  if (ancestor_ == nullptr) {
    return this;
  }
  // If ancestor is not the root, we can recursively evaluate the ancestor
  // If the ancestor's ancestor is nullptr, it means the ancestor is the root,
  // we can directly return the label
  if (ancestor_->ancestor_ != nullptr) {
    ancestor_->Eval();
    if (ancestor_->label_->semi_dominator_->dfs_order_ < label_->semi_dominator_->dfs_order_) {
      label_ = ancestor_->label_;
    }
    ancestor_ = ancestor_->ancestor_;
  }
  return label_;
}

auto DomTreeNode::AddBucket(DomTreeNode *node) -> void { bucket_.emplace_back(node); }

auto DomTreeNode::ProcessBucket() -> void {
  for (auto *node : bucket_) {
    auto *y = node->Eval();
    if (y->semi_dominator_ == node->semi_dominator_) {
      node->idom_ = this;
    } else {
      node->idom_ = y;
    }
  }
  bucket_.clear();
}

// DomTree implementation
auto DomTree::CalculateSemiDominator() -> void {
  if (nodes_.size() <= 1) {
    return;
  }
  // Tranverse the nodes in reverse DFS order, and calculate the semi-dominator for each node
  for (size_t i = nodes_.size(); i-- > 1;) {
    auto *node = nodes_[i];
    auto predecessors = helper_->GetPredecessors(node->cfg_node_);
    for (auto *pred : predecessors) {
      auto iter = cfg_node_to_dom_tree_node_.find(pred);
      if (iter == cfg_node_to_dom_tree_node_.end()) {
        continue;
      }
      auto *pred_node = iter->second;
      auto *u = pred_node->Eval();
      if (u->semi_dominator_->dfs_order_ < node->semi_dominator_->dfs_order_) {
        node->semi_dominator_ = u->semi_dominator_;
      }
    }
    node->semi_dominator_->AddBucket(node);
    node->Link();
    if (node->parent_ != nullptr) {
      node->parent_->ProcessBucket();
    }
  }
}

auto DomTree::CalculateImmediateDominator() -> void {
  if (nodes_.empty()) {
    return;
  }
  // Tranverse the nodes in DFS order again,
  // and update the immediate dominator for each node
  for (size_t i = 1; i < nodes_.size(); ++i) {
    auto *node = nodes_[i];
    if (node->idom_ != node->semi_dominator_) {
      node->idom_ = node->idom_->idom_;
    }
  }
  for (auto *node : nodes_) {
    node->children_.clear();
  }
  for (size_t i = 1; i < nodes_.size(); ++i) {
    auto *node = nodes_[i];
    if (node->idom_ != nullptr) {
      node->idom_->children_.emplace_back(node);
    }
  }
}

auto DomTree::Initialize(CFGNode *cfg_node, DomTreeNode *parent) -> void {
  // Initially the cfg_node is the root
  // If not visited, we can create a new DomTreeNode for the cfg_node,
  // and set the parent-child relationship
  auto dom_node = new DomTreeNode(nodes_.size(), cfg_node);
  dom_node->parent_ = parent;
  // Add the new DomTreeNode to the nodes_ vector and the mapping
  nodes_.emplace_back(dom_node);
  cfg_node_to_dom_tree_node_[cfg_node] = dom_node;
  dom_tree_node_to_cfg_node_[dom_node] = cfg_node;

  // Traverse the successors of the cfg_node, and recursively initialize the DomTreeNode for each successor
  auto successors = helper_->GetSuccessors(cfg_node);
  for (auto *succ : successors) {
    if (cfg_node_to_dom_tree_node_.count(succ) == 0) {
      // Not visited
      Initialize(succ, dom_node);
    }
  }
}

// Build the dominator tree using the Lengauer-Tarjan algorithm
auto DomTree::BuildDomTree(FunctionCFG *function_cfg) -> void {
  // DFS the CFG and number the nodes in DFS order
  auto entry_node = helper_->GetEntryNode(function_cfg);
  if (entry_node == nullptr) {
    // Meet error
    compiler_context_->Print("Entry node is nullptr, cannot build dominator tree\n");
    return;
  }
  // Initialize the DomTreeNode for each CFGNode,
  // and set the parent-child relationship
  Initialize(entry_node);
  this->entry_node_ = cfg_node_to_dom_tree_node_[entry_node];

  // Calculate the semi-dominator for each node
  CalculateSemiDominator();

  // Calculate the immediate dominator for each node
  CalculateImmediateDominator();

  // Build the post order for the domtree,
  BuildPostOrder(entry_node_);

// Print for debug
// #define DEBUG
#ifdef DEBUG
  // Print the dominator tree for debug
  printf("Dominator Tree for %s:\n", GetFunctionDebugName(function_cfg).c_str());
  for (auto &node : nodes_) {
    auto node_name = GetLabelDebugName(function_cfg, node->GetCFGNode());
    auto idom_name =
        node->idom_ == nullptr ? std::string("nullptr") : GetLabelDebugName(function_cfg, node->idom_->GetCFGNode());
    auto semi_name = GetLabelDebugName(function_cfg, node->semi_dominator_->GetCFGNode());
    printf("%s: idom = %s, semi_dominator = %s\n", node_name.c_str(), idom_name.c_str(), semi_name.c_str());
  }
#endif
}

// Build the post order for the domtree,
// which will be used in some optimizations like code motion
auto DomTree::BuildPostOrder(DomTreeNode *node) -> void {
  if (node == entry_node_) {
    post_order_.clear();
  }
  // A tree, never has cycle
  for (auto &child : node->children_) {
    BuildPostOrder(child);
  }
  post_order_.emplace_back(node);
}

// DomTreePass implementation
auto DomTreePass::Run() -> void {
  // Check cfg pass
  PreRun();
  // Get the CFG information from the CFG pass
  auto cfg_pass = GET_PASS_CONTEXT()->GetSpecPass<CFGPass>("CFGPass");
  if (cfg_pass == nullptr) {
    compiler_context_->ReportError("CFGPass is not found, cannot build dominator tree\n");
    return;
  }
  auto function_cfgs = cfg_pass->Get();
  // Build the dominator tree for each function CFG
  auto normalhelper = new NormalDomTreeHelper();

  // Not implemented yet, since we do not have the unify pass to unify the exit nodes,
  //   auto posthelper = new PostDomTreeHelper();
  for (auto &function_cfg : function_cfgs) {
    auto dom_tree = new DomTree(compiler_context_, normalhelper);
    // auto post_dom_tree = new DomTree(compiler_context_, posthelper);
    dom_tree->BuildDomTree(function_cfg);
    // post_dom_tree->BuildDomTree(function_cfg);
    dom_tree_[function_cfg] = dom_tree;
    // post_dom_tree_[function_cfg] = post_dom_tree;
  }

  // Validate the pass after building the dominator tree,
  // since some passes might depend on the dominator tree information,
  Validate();
}
}  // namespace middleend::pass::analysis
