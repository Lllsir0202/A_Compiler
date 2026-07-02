#ifndef MIDDLEEND_PASS_ANALYSIS_MEMORYSSA_MEMORYSSA_H
#define MIDDLEEND_PASS_ANALYSIS_MEMORYSSA_MEMORYSSA_H

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "middleend/pass/analysis/analysis.h"

namespace middleend::mlir {
class Block;
class Operation;
class Value;
}  // namespace middleend::mlir

namespace middleend::pass::analysis {
class AliasAnalysisPass;
class CFGNode;
class DomTree;
class DomTreeNode;
class FunctionCFG;
class MLIRFunctionCFG;

/**
 * @brief A node of the memory SSA graph.
 *
 * Memory is modelled as a single SSA value. A `MemoryDef` is a write (a `var.set` store or
 * a clobbering call), plus one sentinel live-on-entry def per function. A `MemoryUse` is a
 * `var.getval` load. A `MemoryPhi` merges the reaching memory states at a control-flow join.
 * Every use/def/phi records its reaching definition, so a load can be traced to the write
 * whose value it observes — across blocks, and correctly through loops via the phi placed at
 * the loop header.
 */
class MemoryAccess {
 public:
  enum class Kind : uint8_t { Def, Use, Phi };
  MemoryAccess(Kind kind, mlir::Block *block) : kind_(kind), block_(block) {}
  virtual ~MemoryAccess() = default;
  auto GetKind() const -> Kind { return kind_; }
  auto GetBlock() const -> mlir::Block * { return block_; }

 private:
  Kind kind_;
  mlir::Block *block_;
};

// A memory write: a store, a clobbering call, or (when op is nullptr) the live-on-entry sentinel.
class MemoryDef : public MemoryAccess {
 public:
  MemoryDef(mlir::Block *block, mlir::Operation *op, MemoryAccess *defining)
      : MemoryAccess(Kind::Def, block), op_(op), defining_(defining) {}
  auto GetOperation() const -> mlir::Operation * { return op_; }
  auto GetDefiningAccess() const -> MemoryAccess * { return defining_; }
  auto IsLiveOnEntry() const -> bool { return op_ == nullptr; }

 private:
  mlir::Operation *op_;
  MemoryAccess *defining_;
};

// A memory read (a load), pointing at its reaching definition.
class MemoryUse : public MemoryAccess {
 public:
  MemoryUse(mlir::Block *block, mlir::Operation *op, MemoryAccess *defining)
      : MemoryAccess(Kind::Use, block), op_(op), defining_(defining) {}
  auto GetOperation() const -> mlir::Operation * { return op_; }
  auto GetDefiningAccess() const -> MemoryAccess * { return defining_; }

 private:
  mlir::Operation *op_;
  MemoryAccess *defining_;
};

// A merge of reaching memory states, one per (pred block, reaching access) pair.
class MemoryPhi : public MemoryAccess {
 public:
  explicit MemoryPhi(mlir::Block *block) : MemoryAccess(Kind::Phi, block) {}
  auto AddIncoming(mlir::Block *predecessor, MemoryAccess *access) -> void {
    incomings_.emplace_back(predecessor, access);
  }
  auto GetIncomings() const -> const std::vector<std::pair<mlir::Block *, MemoryAccess *>> & { return incomings_; }

 private:
  std::vector<std::pair<mlir::Block *, MemoryAccess *>> incomings_;
};

class MemorySSAPass : public AnalysisPass {
 public:
  explicit MemorySSAPass(context::CompilerContext *compiler_context, PassStage stage)
      : AnalysisPass(compiler_context, stage) {
    AddDependency("CFGPass");
    AddDependency("DomTreePass");
    AddDependency("AliasAnalysisPass");
  }
  ~MemorySSAPass() override { ClearState(); }

  auto Run() -> void override;
  auto Name() const -> std::string override { return "MemorySSAPass"; }
  auto IsAnalysisPass() -> bool override { return true; }

  // The memory access attached to a store/call/load operation, or nullptr.
  auto GetMemoryAccess(mlir::Operation *operation) -> MemoryAccess *;
  // The nearest access that may clobber the value read by `load`, refined by alias analysis:
  // no-alias definitions are skipped and phi paths that all agree are looked through.
  auto GetClobberingAccess(mlir::Operation *load) -> MemoryAccess *;

 private:
  auto RunOnFunction(MLIRFunctionCFG *function_cfg, DomTree *dom_tree) -> void;
  auto ComputeDominanceFrontier(MLIRFunctionCFG *function_cfg, DomTree *dom_tree) -> void;
  auto PlacePhis(MLIRFunctionCFG *function_cfg) -> void;
  auto Rename(DomTreeNode *dom_node, MemoryAccess *incoming) -> void;
  auto WalkClobber(MemoryAccess *access, mlir::Value *pointer,
                   std::unordered_set<MemoryAccess *> *visited) -> MemoryAccess *;
  auto DefClobbers(MemoryDef *def, mlir::Value *pointer) -> bool;
  auto Idom(DomTree *dom_tree, CFGNode *node) -> CFGNode *;
  auto ClearState() -> void;

  AliasAnalysisPass *alias_analysis_{nullptr};
  std::vector<MemoryAccess *> owned_accesses_;
  std::unordered_map<mlir::Operation *, MemoryAccess *> op_to_access_;
  std::unordered_map<mlir::Block *, MemoryPhi *> block_to_phi_;
  std::unordered_map<CFGNode *, std::unordered_set<CFGNode *>> dominance_frontier_;
};

}  // namespace middleend::pass::analysis

#endif
