#ifndef MIDDLEEND_PASS_TRANSFORM_GVNPRE_GVNPRE_H
#define MIDDLEEND_PASS_TRANSFORM_GVNPRE_GVNPRE_H

#include <cstdint>
#include <stdexcept>
#include <typeinfo>
#include <unordered_map>
#include <vector>
#include "middleend/pass/transform/transform.h"

namespace middleend::mlir {
class Block;
class Operation;
class Value;
}  // namespace middleend::mlir

namespace middleend::pass::analysis {
class DomTree;
class DomTreeNode;
class MLIRFunctionCFG;
}  // namespace middleend::pass::analysis

namespace middleend::pass::transform {

/**
 * @brief Partial-redundancy elimination (the "PRE" half of GVN-PRE, after
 * VanDrunen & Hosking, "Value-Based Partial Redundancy Elimination"; the same
 * algorithm the referenced Gnalc compiler implements as gvn_pre).
 *
 * The dominator-scoped @ref GVNPass only removes a computation that is redundant
 * on *every* path to it (its leader dominates). PRE catches the computation that
 * is redundant on *some* paths: an expression `e` recomputed at a merge block M
 * whose value is already available along one or more predecessors of M. For each
 * such e, e is hoisted into the predecessors that lack it (a pure, non-faulting
 * op with operands available there), a phi in M merges the per-predecessor
 * values, and the recomputation of e in M is replaced by that phi -- turning a
 * partial redundancy into a fully removed one.
 *
 * This runs after GVN so operand values are already congruence leaders. It is
 * deliberately conservative for correctness: only pure scalar arithmetic /
 * comparison / unary / cast expressions whose operands each either dominate M or
 * are phis in M (single-level phi translation), only true partial redundancies
 * (available in >= 1 predecessor), and it never hoists across a critical edge.
 * Anything else is left untouched, so no per-input special-casing is needed.
 */
class GVNPREPass : public TransformPass {
 public:
  GVNPREPass(context::CompilerContext *compiler_context, PassStage stage) : TransformPass(compiler_context, stage) {
    if (stage != PassStage::BASEIR) {
      throw std::invalid_argument("GVNPREPass only supports BASEIR stage");
    }
    AddDependency("DomTreePass");
    AddDependency("CFGPass");
  }
  ~GVNPREPass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "GVNPREPass"; }

  /// Value-numbering key of a pure operation: two operations with an equal key
  /// compute the same value (operands are congruence leaders after GVN).
  struct Expr {
    const std::type_info *opcode{nullptr};
    bool has_immediate{false};
    uint64_t immediate{0};
    std::vector<mlir::Value *> operands;

    auto operator==(const Expr &other) const -> bool {
      return has_immediate == other.has_immediate && immediate == other.immediate && operands == other.operands &&
             *opcode == *other.opcode;
    }
  };
  struct ExprHash {
    auto operator()(const Expr &expr) const -> size_t {
      size_t hash = expr.opcode->hash_code();
      auto mix = [&hash](size_t value) { hash ^= value + 0x9e3779b9 + (hash << 6) + (hash >> 2); };
      mix(std::hash<bool>()(expr.has_immediate));
      mix(std::hash<uint64_t>()(expr.immediate));
      for (auto *operand : expr.operands) {
        mix(std::hash<mlir::Value *>()(operand));
      }
      return hash;
    }
  };

 private:
  auto RunOnFunction(analysis::MLIRFunctionCFG *function_cfg, analysis::DomTree *dom_tree) -> bool;
  void BuildOpToBlock(analysis::MLIRFunctionCFG *function_cfg);
  void BuildAvailOut(analysis::DomTreeNode *node,
                     const std::unordered_map<Expr, mlir::Value *, ExprHash> &parent_avail);
  auto TryPRE(mlir::Block *merge, analysis::MLIRFunctionCFG *function_cfg, analysis::DomTree *dom_tree) -> bool;

  auto ComputeKey(mlir::Operation *operation, Expr *key) -> bool;
  auto IsPurePRE(mlir::Operation *operation) -> bool;
  auto IsCommutative(mlir::Operation *operation) -> bool;
  auto MakeOp(mlir::Operation *sample, mlir::Value *res, const std::vector<mlir::Value *> &operands)
      -> mlir::Operation *;
  // Value defined by `value` is available at the end of block `where` (its def dominates `where`).
  auto AvailableAt(mlir::Value *value, mlir::Block *where, analysis::MLIRFunctionCFG *cfg, analysis::DomTree *dom)
      -> bool;
  auto BlockOf(mlir::Value *value) -> mlir::Block *;

  std::unordered_map<mlir::Operation *, mlir::Block *> op_to_block_;
  std::unordered_map<mlir::Block *, std::unordered_map<Expr, mlir::Value *, ExprHash>> avail_out_;
  std::unordered_map<mlir::Operation *, bool> inserted_;  // ops this pass created (never re-processed)
  std::unordered_map<mlir::Value *, bool> dead_values_;   // results of ops this pass removed (stale in avail_out)
  bool changed_{false};
};

}  // namespace middleend::pass::transform

#endif
