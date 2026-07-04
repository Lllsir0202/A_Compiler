#ifndef MIDDLEEND_MLIR_UTILS_CLONE_OPERATIONCLONER_H
#define MIDDLEEND_MLIR_UTILS_CLONE_OPERATIONCLONER_H

#include <unordered_map>

namespace context {
class CompilerContext;
}

namespace middleend::mlir {
class Block;
class Operation;
class Region;
class Value;
}  // namespace middleend::mlir

namespace middleend::mlir::utils {

/**
 * @brief Deep-clones MLIR operations, blocks, and regions while remapping operands through a
 * caller-visible value map. Callers seed `value_map_` with the substitutions they want (e.g.
 * function arguments to call operands, or loop-carried block arguments to per-iteration values)
 * and every cloned result is registered in the map so later references remap consistently.
 *
 * The cloner reproduces the scf.while invariant that the condition and body blocks share the same
 * loop-carried block-argument Value objects: each shared argument is cloned exactly once and
 * attached to both cloned blocks. Cloning does NOT register uses on the produced operations;
 * callers that rely on use-lists must rebuild them afterwards.
 */
class OperationCloner {
 public:
  explicit OperationCloner(context::CompilerContext *compiler_context) : compiler_context_(compiler_context) {}

  // Substitution table (original value -> cloned/replacement value). Seed it before cloning; it is
  // updated in place as results are cloned. Clear it between independent cloning contexts.
  std::unordered_map<mlir::Value *, mlir::Value *> value_map_;

  auto Clone(mlir::Operation *operation) -> mlir::Operation *;
  auto CloneRegion(mlir::Region *region) -> mlir::Region *;
  auto CloneBlock(mlir::Block *block) -> mlir::Block *;
  // Creates a fresh Value carrying the same type/name as `original` and records the mapping.
  auto CloneResult(mlir::Value *original) -> mlir::Value *;
  // Returns the substitution for `value` if one exists, otherwise `value` unchanged.
  auto RemapValue(mlir::Value *value) -> mlir::Value *;

 private:
  context::CompilerContext *compiler_context_;
};

}  // namespace middleend::mlir::utils

#endif
