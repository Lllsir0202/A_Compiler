#ifndef MIDDLEEND_PASS_ANALYSIS_CFG_CFG_H
#define MIDDLEEND_PASS_ANALYSIS_CFG_CFG_H
#include <vector>
#include "middleend/pass/pass.h"
namespace middleend::pass::analysis {
enum class CFGType { MLIR, LLVMIR };

class CFGNode {
 public:
  // Don't allow create CFGNode
  virtual ~CFGNode() = 0;
  /**
   * @brief GetSuccessors and GetPredecessors are used to
   * get the successors and predecessors of the current node in the CFG
   */
  auto GetSuccessors() -> std::vector<CFGNode *> { return successors_; }
  auto GetPredecessors() -> std::vector<CFGNode *> { return predecessors_; }

  auto AddSuccessor(CFGNode *successor) -> void { successors_.emplace_back(successor); }
  auto AddPredecessor(CFGNode *predecessor) -> void { predecessors_.emplace_back(predecessor); }

  virtual auto VerifyNodeType() -> bool = 0;

 protected:
  explicit CFGNode(CFGType node_type) : node_type_(node_type) {}
  std::vector<CFGNode *> successors_;
  std::vector<CFGNode *> predecessors_;
  // Verify node type
  const CFGType node_type_;
  // Implement the CFGNode for Block and BasicBlock
};

// Each function has a CFG
class FunctionCFG {
 public:
  virtual ~FunctionCFG() = 0;
  auto GetNodes() -> std::vector<CFGNode *> { return nodes_; }
  auto AddNode(CFGNode *node) -> void { nodes_.emplace_back(node); }
  // SetEntryNode and GetEntryNode are used to
  // get the entry node of the CFG,
  // which is the node that has no predecessor
  auto GetEntryNode() -> CFGNode * { return entry_node_; }
  auto SetEntryNode(CFGNode *entry_node) -> void { entry_node_ = entry_node; }

  auto GetExitNodes() -> std::vector<CFGNode *> { return exit_nodes_; }
  auto AddExitNode(CFGNode *exit_node) -> void { exit_nodes_.emplace_back(exit_node); }

  virtual auto VerifyFunctionCFG() -> bool = 0;

 protected:
  explicit FunctionCFG(CFGType cfg_type) : cfg_type_(cfg_type) {}

  // Type
  const CFGType cfg_type_;

  std::vector<CFGNode *> nodes_;
  CFGNode *entry_node_;
  // Might be a list of exit nodes, we will use it in the future
  // to unify the return point
  std::vector<CFGNode *> exit_nodes_;
  // Add information to recognize the function in concrete class
};

// Used to build CFG
class CFGBuilder {
 public:
  virtual ~CFGBuilder() = 0;
  virtual auto BuildCFG() -> void = 0;
  auto GetFunctionCFGs() -> std::vector<FunctionCFG *> { return function_cfgs_; }

 protected:
  explicit CFGBuilder(context::CompilerContext *compiler_context) : compiler_context_(compiler_context) {}
  context::CompilerContext *compiler_context_;
  std::vector<FunctionCFG *> function_cfgs_;
};

class CFGPass : public Pass {
 public:
  explicit CFGPass(context::CompilerContext *compiler_context, CFGBuilder *cfg_builder)
      : Pass(compiler_context), cfg_builder_(cfg_builder) {}
  auto Run() -> void override { cfg_builder_->BuildCFG(); }
  ~CFGPass() override { delete cfg_builder_; }

 protected:
  CFGBuilder *cfg_builder_;
};

}  // namespace middleend::pass::analysis

#endif