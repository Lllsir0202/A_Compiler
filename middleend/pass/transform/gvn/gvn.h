#ifndef MIDDLEEND_PASS_TRANSFORM_GVN_GVN_H
#define MIDDLEEND_PASS_TRANSFORM_GVN_GVN_H

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
class AliasAnalysisPass;
class DomTree;
class DomTreeNode;
class MemoryAccess;
class MemorySSAPass;
class MLIRFunctionCFG;
}  // namespace middleend::pass::analysis

namespace middleend::pass::transform {

/**
 * @brief Dominator-tree based global value numbering / common subexpression elimination,
 * extended with MemorySSA-driven load/store forwarding.
 *
 * Scalar redundancy:
 * The pass walks the dominator tree in pre-order while maintaining a scoped table of
 * available expressions. Every pure operation is described by an @ref Expr key built
 * from its opcode plus its operand value numbers (the operand `Value *` pointers, which
 * are already the leaders of their congruence class thanks to the dominator order). When
 * the key of an operation is already present in an enclosing scope, the operation is
 * redundant: its result is replaced by the dominating leader value and the operation is
 * removed. Copy operations are folded away by forwarding their source.
 *
 * Memory redundancy (load/store forwarding), driven by @ref MemorySSAPass:
 * Every load is asked for its clobbering memory access (the nearest write whose value it
 * observes, computed with alias refinement across blocks and through loops). If that clobber
 * is a store to a must-alias pointer the stored value is forwarded directly. Otherwise two
 * loads sharing the same clobber and pointer read the same value, so the dominated one is
 * replaced by the dominating one. Because MemorySSA already accounts for merges and loop
 * back-edges, no conservative memory barrier is needed and load reuse works across blocks.
 *
 * On unwinding a dominator scope the entries inserted inside it are removed, so an available
 * value is only reused where its definition dominates the use. This is general and correct
 * without any per-input special-casing.
 */
class GVNPass : public TransformPass {
 public:
  GVNPass(context::CompilerContext *compiler_context, PassStage stage) : TransformPass(compiler_context, stage) {
    if (stage != PassStage::BASEIR) {
      throw std::invalid_argument("GVNPass only supports BASEIR stage");
    }
    AddDependency("DomTreePass");
    AddDependency("CFGPass");
    AddDependency("AliasAnalysisPass");
    AddDependency("MemorySSAPass");
  }
  ~GVNPass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "GVNPass"; }

  /**
   * @brief Value-numbering key of a pure operation.
   * Two operations sharing an equal key are guaranteed to compute the same value.
   */
  struct Expr {
    const std::type_info *opcode{nullptr};  // dynamic type of the operation, distinguishes op kinds
    bool has_immediate{false};              // constants fold their literal into the key
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

  /**
   * @brief Availability key of a load: the clobbering memory access it reads from together
   * with its pointer. Two loads with an equal key observe the same value.
   */
  struct LoadKey {
    analysis::MemoryAccess *clobber{nullptr};
    mlir::Value *pointer{nullptr};

    auto operator==(const LoadKey &other) const -> bool { return clobber == other.clobber && pointer == other.pointer; }
  };

  struct LoadKeyHash {
    auto operator()(const LoadKey &key) const -> size_t {
      return std::hash<void *>()(key.clobber) ^ (std::hash<void *>()(key.pointer) << 1);
    }
  };

 private:
  auto RunOnFunction(analysis::MLIRFunctionCFG *function_cfg, analysis::DomTree *dom_tree) -> bool;
  auto ProcessDomNode(analysis::DomTreeNode *dom_node) -> void;
  auto ProcessBlock(mlir::Block *block, std::vector<Expr> *scalar_scope, std::vector<LoadKey> *load_scope) -> void;
  auto PropagateCopy(mlir::Operation *operation) -> void;
  auto EliminateRedundant(mlir::Operation *operation, mlir::Value *leader) -> void;
  auto ForwardLoad(mlir::Operation *operation, mlir::Value *value) -> void;
  auto TryForwardLoad(mlir::Operation *load, std::vector<LoadKey> *load_scope) -> bool;
  auto ComputeKey(mlir::Operation *operation, Expr *key) -> bool;
  auto IsPure(mlir::Operation *operation) -> bool;
  auto IsCommutative(mlir::Operation *operation) -> bool;

  std::unordered_map<Expr, mlir::Value *, ExprHash> available_values_;
  std::unordered_map<LoadKey, mlir::Value *, LoadKeyHash> available_loads_;
  analysis::AliasAnalysisPass *alias_analysis_{nullptr};
  analysis::MemorySSAPass *memory_ssa_{nullptr};
  bool changed_{false};
};

}  // namespace middleend::pass::transform

#endif
