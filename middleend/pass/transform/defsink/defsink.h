#ifndef MIDDLEEND_PASS_TRANSFORM_DEFSINK_DEFSINK_H
#define MIDDLEEND_PASS_TRANSFORM_DEFSINK_DEFSINK_H

#include <stdexcept>
#include <unordered_set>
#include <vector>
#include "middleend/pass/transform/transform.h"

namespace middleend::mlir {
class Block;
class Operation;
}  // namespace middleend::mlir

namespace middleend::mlir::var {
class VarDefOp;
}  // namespace middleend::mlir::var

namespace middleend::pass::analysis {
class DomTreePass;
class LoopInfoPass;
class MLIRFunctionCFG;
}  // namespace middleend::pass::analysis

namespace middleend::pass::transform {

/**
 * @brief Sink a local aggregate's definition to the deepest block that still dominates every use.
 *
 * Only the *initialization* moves. The storage itself is a stack slot allocated once per frame, so
 * an aggregate whose definition sits in the entry block pays its zero-fill (or initializer memcpy)
 * on every entry to the function, even on paths that never touch it. A recursive function with an
 * early return is the case that hurts: the fill runs on every call, while the vast majority of calls
 * bail out at the guard.
 *
 * Legality comes down to two conditions on the target block:
 *  - it dominates every use of the definition's result, so no use ever reads uninitialized memory;
 *  - it is not inside a loop that the original definition sits outside of. Re-running the fill on
 *    each iteration would both cost more and, worse, wipe values written by earlier iterations.
 *
 * The definition is relocated between existing blocks only; no terminator is touched, so the CFG,
 * dominator tree and loop forest stay valid.
 */
class VarDefSinkPass : public TransformPass {
 public:
  VarDefSinkPass(context::CompilerContext *compiler_context, PassStage stage)
      : TransformPass(compiler_context, stage) {
    if (stage != PassStage::BASEIR) {
      throw std::invalid_argument("VarDefSinkPass only supports BASEIR stage");
    }
    AddDependency("CFGPass");
    AddDependency("DomTreePass");
    AddDependency("LoopInfoPass");
  }
  ~VarDefSinkPass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "VarDefSinkPass"; }

 private:
  auto RunOnFunction(analysis::MLIRFunctionCFG *function_cfg) -> bool;
  // The deepest block on the dominator path from `from` that still dominates every use of `def_op`
  // and does not sink the definition into a loop. Returns `from` when nothing better exists.
  auto FindSinkTarget(analysis::MLIRFunctionCFG *function_cfg, mlir::var::VarDefOp *def_op,
                      mlir::Block *from) -> mlir::Block *;

  analysis::DomTreePass *dom_tree_pass_{nullptr};
  analysis::LoopInfoPass *loop_info_pass_{nullptr};
  int sunk_count_{0};
};

}  // namespace middleend::pass::transform

#endif
