#include "middleend/pass/analysis/domtree/domtree.h"
#include "context/compiler_context.h"
#include "middleend/pass/analysis/cfg/cfg.h"
#define GET_PASS_CONTEXT() (compiler_context_->GetPassContext())
namespace middleend::pass::analysis {
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
  auto &bucket = parent_->bucket_;
  for (auto *node : bucket) {
    auto *y = node->Eval();
    if (y == nullptr) {
      printf("ProcessBucket: node %zu, eval returns nullptr\n", node->dfs_order_);
    }
    if (y->semi_dominator_ == node->semi_dominator_) {
      node->idom_ = node->parent_;
    } else {
      node->idom_ = y;
    }
  }
  bucket_.clear();
}

// DomTree implementation
auto DomTree::CalculateSemiDominator() -> void {
  // Tranverse the nodes in reverse DFS order, and calculate the semi-dominator for each node
  for (int i = nodes_.size() - 1; i > 0; --i) {
    auto *node = nodes_[i];
    auto predecessors = helper_->GetPredecessors(node->cfg_node_);
    for (auto *pred : predecessors) {
      auto *pred_node = cfg_node_to_dom_tree_node_[pred];
      auto *u = pred_node->Eval();
      if (u->semi_dominator_->dfs_order_ < node->semi_dominator_->dfs_order_) {
        node->semi_dominator_ = u->semi_dominator_;
      }
    }
    node->semi_dominator_->AddBucket(node);
  }
}

auto DomTree::CalculateImmediateDominator() -> void {
  // Tranverse the nodes in DFS order, and calculate the immediate dominator for each node
  if (nodes_.empty()) {
    return;
  }
  for (int i = nodes_.size() - 1; i > 0; --i) {
    auto *node = nodes_[i];
    node->ProcessBucket();
  }
  // Tranverse the nodes in DFS order again,
  // and update the immediate dominator for each node
  for (auto &node : nodes_) {
    // If the immediate dominator is entry nide,
    // we can skip it
    if (node == entry_node_) {
      continue;
    }
    if (node->idom_ != node->semi_dominator_) {
      node->idom_ = node->idom_->idom_;
    }
  }
}

auto DomTree::Initialize(CFGNode *cfg_node) -> void {
  // Initially the cfg_node is the root
  // If not visited, we can create a new DomTreeNode for the cfg_node,
  // and set the parent-child relationship
  auto dom_node = new DomTreeNode(nodes_.size(), cfg_node);
  // Add the new DomTreeNode to the nodes_ vector and the mapping
  nodes_.emplace_back(dom_node);
  cfg_node_to_dom_tree_node_[cfg_node] = dom_node;

  // Traverse the successors of the cfg_node, and recursively initialize the DomTreeNode for each successor
  auto successors = helper_->GetSuccessors(cfg_node);
  for (auto *succ : successors) {
    if (cfg_node_to_dom_tree_node_.count(succ) == 0) {
      // Not visited
      Initialize(succ);
      // Set the parent-child relationship
      auto *succ_dom_node = cfg_node_to_dom_tree_node_[succ];
      succ_dom_node->parent_ = dom_node;
      dom_node->children_.emplace_back(succ_dom_node);
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

  // Link all nodes
  for (auto &node : nodes_) {
    node->Link();
  }

  // Calculate the semi-dominator for each node
  CalculateSemiDominator();

  // Calculate the immediate dominator for each node
  CalculateImmediateDominator();

// Print for debug
#define DEBUG
#ifdef DEBUG
  // Print the dominator tree for debug
  printf("Dominator Tree:\n");
  for (auto &node : nodes_) {
    printf("Node %zu: ", node->dfs_order_);
    if (node->idom_ != nullptr) {
      printf("idom = %zu, ", node->idom_->dfs_order_);
    } else {
      printf("idom = nullptr, ");
    }
    printf("semi_dominator = %zu\n", node->semi_dominator_->dfs_order_);
  }
#endif
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