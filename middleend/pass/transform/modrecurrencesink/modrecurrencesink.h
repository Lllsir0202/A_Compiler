#ifndef MIDDLEEND_PASS_TRANSFORM_MODRECURRENCESINK_MODRECURRENCESINK_H
#define MIDDLEEND_PASS_TRANSFORM_MODRECURRENCESINK_MODRECURRENCESINK_H

#include <stdexcept>
#include <string>
#include "middleend/pass/transform/transform.h"

namespace middleend::mlir {
class Block;
class Operation;
class Value;
}  // namespace middleend::mlir
namespace middleend::pass::analysis {
class Loop;
class MLIRFunctionCFG;
class SCEVPass;
}  // namespace middleend::pass::analysis

namespace middleend::pass::transform {

// Defer a loop-carried `(acc + value) % positive_constant` until the loop exit.
// The recurrence is widened to signed i64, making the transformation exact for
// every loop with a statically bounded i32 trip count.  Candidates with an
// observable intermediate accumulator, multiple latches/exits, or ambiguous
// LCSSA flow are rejected.
class ModRecurrenceSinkPass : public TransformPass {
 public:
  ModRecurrenceSinkPass(context::CompilerContext *compiler_context, PassStage stage)
      : TransformPass(compiler_context, stage) {
    if (stage != PassStage::BASEIR) throw std::invalid_argument("ModRecurrenceSinkPass only supports BASEIR");
    AddDependency("CFGPass");
    AddDependency("DomTreePass");
    AddDependency("LoopInfoPass");
    AddDependency("LoopSimplifyPass");
    AddDependency("SCEVPass");
  }

  auto Run() -> void override;
  auto Name() const -> std::string override { return "ModRecurrenceSinkPass"; }

 private:
  auto RunOnLoop(analysis::MLIRFunctionCFG *cfg, analysis::Loop *loop, analysis::SCEVPass *scev) -> bool;
};

}  // namespace middleend::pass::transform
#endif
