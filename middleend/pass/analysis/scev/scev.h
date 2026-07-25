#ifndef MIDDLEEND_PASS_ANALYSIS_SCEV_SCEV_H
#define MIDDLEEND_PASS_ANALYSIS_SCEV_SCEV_H
#include <cstdint>
#include <variant>
#include <vector>
#include "utils/type/type.h"

namespace middleend::mlir {
class Value;
}

namespace middleend::pass::analysis {
class Loop;

/**
 * @brief The kind of a scalar-evolution expression. Kept as an explicit tag so callers can switch
 * on it, while `DynamicOrNull`-style dynamic_cast still works for typed access; this mirrors how
 * the rest of the IR identifies operations.
 */
enum class SCEVKind : uint8_t { Constant, Unknown, Add, Mul, AddRec, Div, RepeatedDiv, SMax, UMin };

/**
 * @brief Base class of every scalar-evolution expression. Nodes are immutable after construction,
 * structurally uniqued (so pointer identity means semantic equality), and owned by the SCEVPass,
 * which frees them in ClearState(). A node never deletes itself, matching the middleend convention
 * that resource ownership is centralized rather than scattered across the graph.
 */
class SCEV {
 public:
  virtual ~SCEV() = default;
  auto GetKind() const -> SCEVKind { return kind_; }
  auto GetType() const -> type::Type * { return type_; }
  // Stable creation-order id, assigned by SCEVPass when the node is interned.
  // Commutative-operand canonicalization sorts by this instead of by pointer
  // address so the analysis -- and everything downstream of it -- is
  // reproducible across runs (heap addresses vary under ASLR).
  auto GetOrder() const -> size_t { return order_; }
  auto SetOrder(size_t order) -> void { order_ = order; }

 protected:
  SCEV(SCEVKind kind, type::Type *type) : kind_(kind), type_(type) {}
  SCEVKind kind_;
  type::Type *type_;
  size_t order_ = 0;
};

/**
 * @brief A compile-time constant, carrying the same int/float variant as base::ConstIntOp and
 * base::ConstFloatOp. Only integer constants participate in recurrence folding; float constants are
 * kept opaque so no reasoning depends on IEEE rounding.
 */
class SCEVConstant : public SCEV {
 public:
  auto GetValue() const -> std::variant<int, float> { return value_; }
  auto IsInt() const -> bool { return std::holds_alternative<int>(value_); }
  auto GetInt() const -> int { return std::get<int>(value_); }

 private:
  friend class SCEVPass;
  SCEVConstant(std::variant<int, float> value, type::Type *type) : SCEV(SCEVKind::Constant, type), value_(value) {}
  std::variant<int, float> value_;
};

/**
 * @brief An opaque leaf: a value the analysis cannot look through (a parameter, a load, a call
 * result, an address, a float value, or a phi that is not an affine recurrence). It still takes part
 * in folding as a symbol, so any loop-invariant subexpression stays visible to consumers.
 */
class SCEVUnknown : public SCEV {
 public:
  auto GetValue() const -> mlir::Value * { return value_; }

 private:
  friend class SCEVPass;
  SCEVUnknown(mlir::Value *value, type::Type *type) : SCEV(SCEVKind::Unknown, type), value_(value) {}
  mlir::Value *value_;
};

/**
 * @brief N-ary commutative expression (sum, product, signed-max or unsigned-min). Operands are
 * stored in a canonical order (uniqued children sorted by pointer), so two structurally equal
 * expressions produce the same node.
 */
class SCEVCommutativeExpr : public SCEV {
 public:
  auto GetOperands() const -> const std::vector<const SCEV *> & { return operands_; }

 protected:
  SCEVCommutativeExpr(SCEVKind kind, type::Type *type, std::vector<const SCEV *> operands)
      : SCEV(kind, type), operands_(std::move(operands)) {}
  std::vector<const SCEV *> operands_;
};

class SCEVAddExpr : public SCEVCommutativeExpr {
  friend class SCEVPass;
  using SCEVCommutativeExpr::SCEVCommutativeExpr;
};

class SCEVMulExpr : public SCEVCommutativeExpr {
  friend class SCEVPass;
  using SCEVCommutativeExpr::SCEVCommutativeExpr;
};

class SCEVSMaxExpr : public SCEVCommutativeExpr {
  friend class SCEVPass;
  using SCEVCommutativeExpr::SCEVCommutativeExpr;
};

class SCEVUMinExpr : public SCEVCommutativeExpr {
  friend class SCEVPass;
  using SCEVCommutativeExpr::SCEVCommutativeExpr;
};

/**
 * @brief A chain of recurrences {a0, +, a1, +, ... , +, ak} on loop L. The value on iteration i is
 * the Newton series sum_j a_j * C(i, j); an affine recurrence is the two-operand case {start, +,
 * step}. Higher-order chains capture, e.g., a running sum of an induction variable. Every operand is
 * invariant in L except that a later operand may itself be a recurrence of an enclosing loop, which
 * is how nested inductions are represented.
 */
class SCEVAddRecExpr : public SCEV {
 public:
  auto GetOperands() const -> const std::vector<const SCEV *> & { return operands_; }
  auto GetStart() const -> const SCEV * { return operands_.front(); }
  auto GetLoop() const -> Loop * { return loop_; }
  auto IsAffine() const -> bool { return operands_.size() == 2; }
  // The step of an affine recurrence; only meaningful when IsAffine().
  auto GetStep() const -> const SCEV * { return operands_[1]; }
  // True when the recurrence is known not to wrap over its iteration range (see SCEVPass).
  auto IsNoWrap() const -> bool { return no_wrap_; }

 private:
  friend class SCEVPass;
  SCEVAddRecExpr(std::vector<const SCEV *> operands, Loop *loop, type::Type *type, bool no_wrap)
      : SCEV(SCEVKind::AddRec, type), operands_(std::move(operands)), loop_(loop), no_wrap_(no_wrap) {}
  std::vector<const SCEV *> operands_;
  Loop *loop_;
  bool no_wrap_;
};

/**
 * @brief Signed division lhs / rhs, produced for base::DivOp and for ceiling-divisions in trip
 * counts. It keeps division visible for index and stride reasoning without asserting any recurrence.
 */
class SCEVDivExpr : public SCEV {
 public:
  auto GetLHS() const -> const SCEV * { return lhs_; }
  auto GetRHS() const -> const SCEV * { return rhs_; }

 private:
  friend class SCEVPass;
  SCEVDivExpr(const SCEV *lhs, const SCEV *rhs, type::Type *type) : SCEV(SCEVKind::Div, type), lhs_(lhs), rhs_(rhs) {}
  const SCEV *lhs_;
  const SCEV *rhs_;
};

/**
 * @brief Repeated signed division by a positive power of two.
 *
 * The recurrence form has no iteration expression and denotes
 * `start / 2^(base_shift + step_shift * i)` in `loop`. EvaluateAtIteration
 * supplies the iteration expression and produces a closed form. Keeping this
 * as a first-class SCEV avoids pretending that truncating division is an
 * additive recurrence while still allowing a dead, value-producing loop to
 * be closed exactly.
 */
class SCEVRepeatedDivExpr : public SCEV {
 public:
  auto GetStart() const -> const SCEV * { return start_; }
  auto GetIteration() const -> const SCEV * { return iteration_; }
  auto GetLoop() const -> Loop * { return loop_; }
  auto GetBaseShift() const -> int { return base_shift_; }
  auto GetStepShift() const -> int { return step_shift_; }
  auto IsClosed() const -> bool { return iteration_ != nullptr; }

 private:
  friend class SCEVPass;
  SCEVRepeatedDivExpr(const SCEV *start, const SCEV *iteration, int base_shift, int step_shift, Loop *loop,
                      type::Type *type)
      : SCEV(SCEVKind::RepeatedDiv, type),
        start_(start),
        iteration_(iteration),
        loop_(loop),
        base_shift_(base_shift),
        step_shift_(step_shift) {}
  const SCEV *start_;
  const SCEV *iteration_;
  Loop *loop_;
  int base_shift_;
  int step_shift_;
};

}  // namespace middleend::pass::analysis

#endif
