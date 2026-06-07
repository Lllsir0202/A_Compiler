#ifndef MIDDLEEND_PASS_ANALYSIS_DOMTREE_DOMTREE_H
#define MIDDLEEND_PASS_ANALYSIS_DOMTREE_DOMTREE_H

#include <cstddef>
#include <unordered_map>
#include <utility>
#include <vector>
#include "compiler_context.h"
#include "middleend/pass/analysis/analysis.h"
namespace middleend::pass::analysis {
class CFGNode;
class FunctionCFG;
}  // namespace middleend::pass::analysis
namespace middleend::pass::analysis {

/**
 * @brief Since we need to build both the dominator tree and the post-dominator tree,
 * we can just change the Helper class, which is responsible for getting the predecessors and successors
 */
class DomTreeHelper {
 public:
  virtual ~DomTreeHelper() = default;
  // Get the predecessors of a node in the CFG
  virtual auto GetPredecessors(CFGNode *node) -> std::vector<CFGNode *> = 0;
  // Get the successors of a node in the CFG
  virtual auto GetSuccessors(CFGNode *node) -> std::vector<CFGNode *> = 0;
  virtual auto GetEntryNode(FunctionCFG *function_cfg) -> CFGNode * = 0;
};

/**
 * @brief DomTreeNode represents a node in dominator tree
 */
class DomTreeNode {
 public:
  // Destroy the dominator tree node and its children recursively
  ~DomTreeNode() {
    for (DomTreeNode *child : children_) {
      delete child;
    }
  }

  auto GetDFSOrder() const -> size_t { return dfs_order_; }
  auto GetCFGNode() const -> CFGNode * { return cfg_node_; }
  auto GetIDom() const -> DomTreeNode * { return idom_; }
  auto GetChildren() const -> const std::vector<DomTreeNode *> & { return children_; }

 private:
  DomTreeNode(size_t dfs_order, CFGNode *cfg_node) : dfs_order_(dfs_order), cfg_node_(cfg_node) {
    // label are initialized to self for the union-find structure
    ancestor_ = nullptr;
    label_ = this;
    // initialize semi-dominator to self
    semi_dominator_ = this;
    // initialize immediate dominator to nullptr
    idom_ = nullptr;
    // parent to nullptr
    parent_ = nullptr;
  }

  // Allow DomTree to create DomTreeNode instances
  friend class DomTree;

  // Helper functions

  // Union-Find 'link' operation
  auto Link() -> void;
  // Union-Find 'eval' operation
  auto Eval() -> DomTreeNode *;

  // AddBucket adds a node to the bucket of its semi-dominator
  auto AddBucket(DomTreeNode *node) -> void;
  // ProcessBucket processes the bucket of a node after its semi-dominator is determined
  auto ProcessBucket() -> void;

  // DFS order number of the node
  size_t dfs_order_;
  // The node corresponding to the CFGNode
  CFGNode *cfg_node_;
  // The parent node in the DFS spanning tree
  DomTreeNode *parent_;
  // The child nodes in the dominator tree
  std::vector<DomTreeNode *> children_;

  // Following fields are used for the Lengauer-Tarjan algorithm
  // semi-dominator of the node
  DomTreeNode *semi_dominator_;
  // ancestor in the union-find structure
  DomTreeNode *ancestor_;
  // label in the union-find structure
  DomTreeNode *label_;
  // bucket for nodes whose semi-dominator is this node
  std::vector<DomTreeNode *> bucket_;

  // immediate dominator of the node
  DomTreeNode *idom_;
};

class DomTree {
 public:
  explicit DomTree(context::CompilerContext *compiler_context, DomTreeHelper *helper)
      : helper_(helper), compiler_context_(compiler_context) {}
  ~DomTree() {
    delete entry_node_;
    delete helper_;
  }

  // Interface to get the root node of the dominator tree
  auto GetEntryNode() -> DomTreeNode * { return entry_node_; }
  auto GetNodes() -> std::vector<DomTreeNode *> { return nodes_; }
  auto GetDomTreeNode(CFGNode *cfg_node) -> DomTreeNode * {
    if (cfg_node_to_dom_tree_node_.count(cfg_node) == 0) {
      return nullptr;
    }
    return cfg_node_to_dom_tree_node_[cfg_node];
  }
  auto Dominates(CFGNode *lhs, CFGNode *rhs) -> bool;

  // DFS to initialize the domtree
  auto Initialize(CFGNode *cfg_node, DomTreeNode *parent = nullptr) -> void;
  // Build the dominator tree using the Lengauer-Tarjan algorithm
  auto BuildDomTree(FunctionCFG *function_cfg) -> void;

  auto BuildPostOrder(DomTreeNode *node) -> void;
  auto GetPostOrder() -> std::vector<DomTreeNode *> { return post_order_; }

 private:
  /**
   * @brief Build the dominator tree using the Lengauer-Tarjan algorithm
   * @param nodes_: The list of DomTreeNodes
   * @note: the index of nodes_ corresponds to the DFS order of the nodes in the CFG
   */
  std::vector<DomTreeNode *> nodes_;

  // Get DomTreeNode by CFGNode, used in the BuildDomTree function
  std::unordered_map<CFGNode *, DomTreeNode *> cfg_node_to_dom_tree_node_;
  std::unordered_map<DomTreeNode *, CFGNode *> dom_tree_node_to_cfg_node_;

  // The root node of the dominator tree.
  DomTreeNode *entry_node_{nullptr};

  // Helper members
  DomTreeHelper *helper_;

  // Helper function to calculate the semi-dominator and immediate dominator for each node in the CFG,
  // used in the BuildDomTree function
  auto CalculateSemiDominator() -> void;
  auto CalculateImmediateDominator() -> void;

  // context for debug printing
  context::CompilerContext *compiler_context_;

  // post order
  std::vector<DomTreeNode *> post_order_;
};

class NormalDomTreeHelper : public DomTreeHelper {
 public:
  ~NormalDomTreeHelper() override = default;
  auto GetPredecessors(CFGNode *node) -> std::vector<CFGNode *> override;
  auto GetSuccessors(CFGNode *node) -> std::vector<CFGNode *> override;
  auto GetEntryNode(FunctionCFG *function_cfg) -> CFGNode * override;
};

class PostDomTreeHelper : public DomTreeHelper {
 public:
  ~PostDomTreeHelper() override = default;
  auto GetPredecessors(CFGNode *node) -> std::vector<CFGNode *> override;
  auto GetSuccessors(CFGNode *node) -> std::vector<CFGNode *> override;
  auto GetEntryNode(FunctionCFG *function_cfg) -> CFGNode * override;
};

class DomTreePass : public AnalysisPass {
 public:
  explicit DomTreePass(context::CompilerContext *compiler_context, PassStage stage)
      : AnalysisPass(compiler_context, stage) {
    AddDependency("CFGPass");
  }
  ~DomTreePass() override = default;
  void Run() override;
  auto Name() const -> std::string override { return "DomTreePass"; }
  auto IsAnalysisPass() -> bool override { return true; }

  // Interfaces to get the dominator tree and post-dominator tree for other passes
  auto GetDomTree(FunctionCFG *function_cfg) -> DomTree * {
    if (dom_tree_.count(function_cfg) > 0) {
      return dom_tree_[function_cfg];
    }
    return nullptr;
  }
  auto GetPostDomTree(FunctionCFG *function_cfg) -> DomTree * {
    if (post_dom_tree_.count(function_cfg) > 0) {
      return post_dom_tree_[function_cfg];
    }
    return nullptr;
  }

 private:
  // Record both domtree and post-domtree
  std::unordered_map<FunctionCFG *, DomTree *> dom_tree_;
  std::unordered_map<FunctionCFG *, DomTree *> post_dom_tree_;
};

}  // namespace middleend::pass::analysis
#endif
