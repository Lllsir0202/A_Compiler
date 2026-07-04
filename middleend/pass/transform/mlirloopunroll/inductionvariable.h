#ifndef MIDDLEEND_PASS_TRANSFORM_MLIRLOOPUNROLL_INDUCTIONVARIABLE_H
#define MIDDLEEND_PASS_TRANSFORM_MLIRLOOPUNROLL_INDUCTIONVARIABLE_H

#include <cstddef>
#include <cstdint>

namespace middleend::mlir {
class Region;
class Value;
}  // namespace middleend::mlir
namespace middleend::mlir::scf {
class WhileOp;
}

namespace middleend::pass::transform::loopiv {

// Loop condition comparison direction, normalized so the induction variable is the left operand.
enum class CmpKind : uint8_t { kLT, kLE, kGT, kGE, kNE, kEQ, kInvalid };
// How the induction variable advances each iteration: `iv + operand` or `iv * operand`.
enum class StepKind : uint8_t { kAdd, kMul };

// Recognized induction variable of a counted while loop. `index_` selects the loop-carried value;
// the loop runs while `cmp(iv, bound_)` holds, with the iv advancing by `step_` each iteration.
struct Induction {
  bool valid_{false};
  size_t index_{0};
  CmpKind kind_{CmpKind::kInvalid};
  StepKind step_kind_{StepKind::kAdd};
  int64_t step_{0};
  mlir::Value *init_{nullptr};   // value entering the loop (may be runtime)
  mlir::Value *bound_{nullptr};  // comparison bound (may be runtime)
};

// Recognizes the induction variable of `while_op`: single-block condition ending in a direct
// comparison, an iv that is a loop-carried block argument advanced by a constant additive or
// multiplicative step through the yield. Returns an Induction with valid_ == false on failure.
auto Recognize(mlir::scf::WhileOp *while_op) -> Induction;

// If `iv.init_` and `iv.bound_` are compile-time constants, returns the exact trip count by
// simulation, or -1 if not constant, larger than `max_trip`, or non-terminating.
auto ConstantTripCount(const Induction &iv, int64_t max_trip) -> int64_t;

// True if `value` is defined inside `while_op` (a loop-carried block argument or an operation
// result in the cond/body regions), i.e. it is NOT loop-invariant.
auto IsDefinedInLoop(mlir::Value *value, mlir::scf::WhileOp *while_op) -> bool;

// True if a loop body region can be cloned to replicate iterations: a single block whose
// operations (recursing through nested `if`) contain no break/continue/return/condition and no
// nested while loop (whose scf.condition the cloner cannot reproduce).
auto BodyIsCloneable(mlir::Region *body) -> bool;

// Total number of operations in a region, recursing into nested regions.
auto CountOperations(mlir::Region *region) -> size_t;

}  // namespace middleend::pass::transform::loopiv

#endif
