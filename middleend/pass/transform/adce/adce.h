#ifndef MIDDLEEND_PASS_TRANSFORM_ADCE_ADCE_H
#define MIDDLEEND_PASS_TRANSFORM_ADCE_ADCE_H

#include <deque>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include "middleend/pass/transform/transform.h"

namespace middleend::mlir {
class Block;
class Operation;
}  // namespace middleend::mlir

namespace middleend::pass::analysis {
class CFGNode;
class DomTree;
class MLIRFunctionCFG;
}  // namespace middleend::pass::analysis

namespace middleend::pass::transform {
class ADCEPass : public TransformPass {
 public:
  ADCEPass(context::CompilerContext *compiler_context, PassStage stage) : TransformPass(compiler_context, stage) {
    if (stage != PassStage::BASEIR) {
      throw std::invalid_argument("ADCEPass only supports BASEIR stage");
    }
    AddDependency("UnifyReturnPass");
    AddDependency("DomTreePass");
  }
  ~ADCEPass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "ADCEPass"; }

 private:
  auto RunOnFunction(analysis::MLIRFunctionCFG *function_cfg, analysis::DomTree *post_dom_tree) -> bool;
  auto CollectRootOperations() -> void;
  auto PropagateLiveness() -> void;
  auto MarkOperationLive(mlir::Operation *operation) -> void;
  auto MarkBlockLive(analysis::CFGNode *cfg_node) -> void;
  auto MarkPhiPredecessorsLive(mlir::Operation *operation) -> void;
  auto MarkControlDependentBranches() -> void;
  auto IsControlDependent(analysis::CFGNode *branch_node, analysis::CFGNode *live_node) -> bool;
  auto RewriteDeadTerminators() -> bool;
  auto RemoveDeadOperations() -> bool;
  auto GetTerminator(mlir::Block *block) -> mlir::Operation *;
  auto GetBlockOfOperation(mlir::Operation *operation) -> mlir::Block *;
  auto FindNearestLivePostDominator(analysis::CFGNode *cfg_node) -> mlir::Block *;
  auto IsAlwaysLive(mlir::Operation *operation) -> bool;
  auto IsTerminator(mlir::Operation *operation) -> bool;
  auto RemoveOperandUses(mlir::Operation *operation) -> void;
  auto ResetFunctionState() -> void;

  analysis::MLIRFunctionCFG *current_function_cfg_{nullptr};
  analysis::DomTree *current_post_dom_tree_{nullptr};
  std::unordered_map<mlir::Operation *, mlir::Block *> operation_to_block_;
  std::unordered_set<mlir::Operation *> live_operations_;
  std::unordered_set<analysis::CFGNode *> live_blocks_;
  std::deque<mlir::Operation *> live_worklist_;
  std::deque<analysis::CFGNode *> new_live_blocks_;
};

class PostSCCPADCEPass : public ADCEPass {
 public:
  PostSCCPADCEPass(context::CompilerContext *compiler_context, PassStage stage) : ADCEPass(compiler_context, stage) {
    AddDependency("SCCPPass");
  }
  ~PostSCCPADCEPass() override = default;

  auto Name() const -> std::string override { return "PostSCCPADCEPass"; }
};

// Runs after GVN/DSE to sweep pure operations (dead getptr/loads) that were left without
// users once loads were forwarded and stores removed.
class PostDSEADCEPass : public ADCEPass {
 public:
  PostDSEADCEPass(context::CompilerContext *compiler_context, PassStage stage) : ADCEPass(compiler_context, stage) {
    AddDependency("DSEPass");
  }
  ~PostDSEADCEPass() override = default;

  auto Name() const -> std::string override { return "PostDSEADCEPass"; }
};

}  // namespace middleend::pass::transform

#endif
