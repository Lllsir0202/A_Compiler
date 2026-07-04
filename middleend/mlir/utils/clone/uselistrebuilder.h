#ifndef MIDDLEEND_MLIR_UTILS_CLONE_USELISTREBUILDER_H
#define MIDDLEEND_MLIR_UTILS_CLONE_USELISTREBUILDER_H

namespace middleend::mlir::func {
class FuncDefOp;
}

namespace middleend::mlir::utils {

/**
 * @brief Rebuilds every value use-list in a function from scratch in two phases: first clear all
 * uses (block arguments, results, nested regions), then re-register operand uses. The two phases
 * must not be interleaved because an scf.while's cond and body blocks share the same loop-carried
 * block-argument Value: clearing one region after a sibling was rebuilt would wipe the sibling's
 * registrations. Passes that clone operations (which does not register uses) or rewrite operands
 * call this to restore consistent use-lists before/after transforming.
 */
auto RebuildFunctionUses(func::FuncDefOp *func_op) -> void;

}  // namespace middleend::mlir::utils

#endif
