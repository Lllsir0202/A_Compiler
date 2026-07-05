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
enum class SCEVKind : uint8_t { Constant, Unknown, Add, Mul, AddRec, Div };

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

 protected:
  SCEV(SCEVKind kind, type::Type *type) : kind_(kind), type_(type) {}
  SCEVKind kind_;
  type::Type *type_;
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
 * @brief N-ary commutative sum or product. Operands are stored in a canonical order (uniqued
 * children sorted by pointer), so two structurally equal expressions produce the same node.
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

/**
 * @brief The add-recurrence {start, +, step}_L: on iteration i of loop L this value equals
 * start + i * step. `start` and `step` are invariant in L (step may itself be an AddRec of an
 * enclosing loop, which is how nested inductions are represented). It is affine when step does not
 * vary in L, which is the only form this analysis constructs.
 */
class SCEVAddRecExpr : public SCEV {
 public:
  auto GetStart() const -> const SCEV * { return start_; }
  auto GetStep() const -> const SCEV * { return step_; }
  auto GetLoop() const -> Loop * { return loop_; }

 private:
  friend class SCEVPass;
  SCEVAddRecExpr(const SCEV *start, const SCEV *step, Loop *loop, type::Type *type)
      : SCEV(SCEVKind::AddRec, type), start_(start), step_(step), loop_(loop) {}
  const SCEV *start_;
  const SCEV *step_;
  Loop *loop_;
};

/**
 * @brief Signed division lhs / rhs, produced for base::DivOp. It keeps division visible for index
 * and stride reasoning without asserting any recurrence, so it never introduces an unsound fold.
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

}  // namespace middleend::pass::analysis

#endif
