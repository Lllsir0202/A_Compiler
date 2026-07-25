#ifndef MIDDLEEND_PASS_TRANSFORM_SCCP_SCCP_H
#define MIDDLEEND_PASS_TRANSFORM_SCCP_SCCP_H

#include <cstdint>
#include <deque>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include "middleend/pass/transform/transform.h"

namespace middleend::mlir {
class Block;
class Operation;
class Value;
}  // namespace middleend::mlir

namespace middleend::pass::analysis {
class CFGNode;
class MLIRFunctionCFG;
}  // namespace middleend::pass::analysis

namespace middleend::pass::transform {
class SCCPPass : public TransformPass {
 public:
  SCCPPass(context::CompilerContext *compiler_context, PassStage stage) : TransformPass(compiler_context, stage) {
    if (stage != PassStage::BASEIR) {
      throw std::invalid_argument("SCCPPass only supports BASEIR stage");
    }
    AddDependency("ADCEPass");
    AddDependency("CFGPass");
  }
  ~SCCPPass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "SCCPPass"; }

  enum class LatticeKind : uint8_t { UNKNOWN, CONSTANT, OVERDEFINED };
  struct ConstantValue {
    std::variant<int, float, bool> value;
  };
  struct LatticeValue {
    LatticeKind kind{LatticeKind::UNKNOWN};
    ConstantValue constant;
  };
  struct CFGEdge {
    analysis::CFGNode *from{nullptr};
    analysis::CFGNode *to{nullptr};

    auto operator==(const CFGEdge &other) const -> bool { return from == other.from && to == other.to; }
  };
  struct CFGEdgeHash {
    auto operator()(const CFGEdge &edge) const -> size_t {
      return std::hash<analysis::CFGNode *>()(edge.from) ^ (std::hash<analysis::CFGNode *>()(edge.to) << 1);
    }
  };

 private:
  auto RunOnFunction(analysis::MLIRFunctionCFG *function_cfg) -> bool;
  auto ResetFunctionState() -> void;
  auto MarkBlockExecutable(analysis::CFGNode *cfg_node) -> void;
  auto MarkEdgeExecutable(analysis::CFGNode *from, analysis::CFGNode *to) -> void;
  auto VisitBlock(analysis::CFGNode *cfg_node) -> void;
  auto VisitOperation(mlir::Operation *operation) -> void;
  auto VisitTerminator(analysis::CFGNode *cfg_node, mlir::Operation *terminator) -> void;
  auto ReplaceConstantsAndBranches() -> bool;
  auto ReplaceConstantValue(mlir::Operation *operation, const LatticeValue &lattice_value) -> bool;
  auto RewriteConstantBranch(analysis::CFGNode *cfg_node, mlir::Operation *terminator) -> bool;
  auto GetValueState(mlir::Value *value) -> LatticeValue;
  auto MergeInValue(mlir::Value *value, const LatticeValue &new_value) -> void;
  auto MergeValues(const LatticeValue &lhs, const LatticeValue &rhs) -> LatticeValue;
  auto MarkUsersChanged(mlir::Value *value) -> void;
  auto MarkOperationChanged(mlir::Operation *operation) -> void;
  auto EvaluateOperation(mlir::Operation *operation) -> LatticeValue;
  auto EvaluatePhi(mlir::Operation *operation) -> LatticeValue;
  auto GetBlockOfOperation(mlir::Operation *operation) -> mlir::Block *;
  auto GetTerminator(mlir::Block *block) -> mlir::Operation *;
  auto IsExecutableEdge(analysis::CFGNode *from, analysis::CFGNode *to) -> bool;
  auto AreEqualConstants(const ConstantValue &lhs, const ConstantValue &rhs) -> bool;
  auto IsTrackedResultOperation(mlir::Operation *operation) -> bool;
  auto IsReplaceableConstant(const LatticeValue &lattice_value) -> bool;
  auto GetBoolConstant(mlir::Value *value) -> std::optional<bool>;
  auto CreateConstantOperation(mlir::Value *result, const ConstantValue &constant) -> mlir::Operation *;

  analysis::MLIRFunctionCFG *current_function_cfg_{nullptr};
  std::unordered_map<mlir::Operation *, mlir::Block *> operation_to_block_;
  std::unordered_map<mlir::Value *, std::vector<mlir::Operation *>> value_users_;
  std::unordered_map<mlir::Block *, analysis::CFGNode *> block_to_node_;
  std::unordered_map<mlir::Value *, LatticeValue> value_states_;
  std::unordered_set<analysis::CFGNode *> executable_blocks_;
  std::unordered_set<CFGEdge, CFGEdgeHash> executable_edges_;
  std::deque<analysis::CFGNode *> cfg_worklist_;
  std::deque<mlir::Operation *> ssa_worklist_;
};

// Second SCCP round after the loop passes: DeadLoopElim materializes closed-form constants and
// the partial unroller's remainder loops often become zero-trip once the main loop's exit values
// are constant, but the first SCCP ran long before any loop pass. This round folds those
// now-constant conditions so the following ADCE can drop the dead loops.
class PostLoopSCCPPass : public SCCPPass {
 public:
  PostLoopSCCPPass(context::CompilerContext *compiler_context, PassStage stage) : SCCPPass(compiler_context, stage) {
    AddDependency("TrivialBlockMergePass");
  }
  ~PostLoopSCCPPass() override = default;

  auto Name() const -> std::string override { return "PostLoopSCCPPass"; }
};

}  // namespace middleend::pass::transform

#endif
