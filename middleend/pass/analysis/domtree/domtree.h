#ifndef MIDDLEEND_PASS_ANALYSIS_DOMTREE_DOMTREE_H
#define MIDDLEEND_PASS_ANALYSIS_DOMTREE_DOMTREE_H

#include <cstddef>
namespace middleend::pass::analysis {
class CFGNode;
}
namespace middleend::pass::analysis {
class DomTreeNode {
 public:
  ~DomTreeNode() = default;

 private:
  DomTreeNode() = default;
  friend class DomTree;
  // DFS order number of the node
  size_t dfs_order_;
  // The node corresponding to the CFGNode
  CFGNode *cfg_node_;
};

class DomTree {
 public:
  explicit DomTree() = default;
  ~DomTree() = default;

 private:
};

}  // namespace middleend::pass::analysis
#endif