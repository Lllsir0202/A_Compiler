#ifndef FRONTEND_AST_VISITOR_SEMANTIC_CALCULATE_H
#define FRONTEND_AST_VISITOR_SEMANTIC_CALCULATE_H
#include <cassert>
#include <stdexcept>
#include "ast/ast_base.h"
#include "ast/ast_utils.h"
#include "frontend/ast/ast_expr.h"
#include "utils/type/type.h"
#define DEBUG

#ifdef DEBUG
#define CHECK(cond, msg) \
  do {                   \
    assert(cond &&msg);  \
  } while (0)
#else
#define CHECK(cond, msg) \
  do {                   \
  } while (0)
#endif

#define BIN_OP_CASE(opcode, expr) \
  case opcode:                    \
    result->GetValue() = expr;    \
    break;
#define UN_OP_CASE(opcode, expr) \
  case opcode:                   \
    result->GetValue() = expr;   \
    break;

namespace frontend::ast {
/**
 * @brief FoldNodes is a helper function to fold constant expressions in semantic analysis phase.
 * @param result
 * @param left
 * @param right
 * @note: this function will only deal with Value type, and we have to handle typeptr
 */
static inline auto FoldNodes(BinaryExprNode *result, ExprNode *left, ExprNode *right) -> void {
  CHECK(result != nullptr, "Result node cannot be null");
  CHECK(left != nullptr, "Left operand cannot be null");
  CHECK(right != nullptr, "Right operand cannot be null");
  CHECK(left->IsConst() && right->IsConst(), "both operands should be constant expressions to merge");
  // We can separately consider different operators
  switch (result->operator_) {
    BIN_OP_CASE(OpCode::PLUS, left->GetValue() + right->GetValue())
    BIN_OP_CASE(OpCode::SUB, left->GetValue() - right->GetValue())
    BIN_OP_CASE(OpCode::MUL, left->GetValue() * right->GetValue())
    BIN_OP_CASE(OpCode::DIV, left->GetValue() / right->GetValue())
    BIN_OP_CASE(OpCode::MOD, left->GetValue() % right->GetValue())
    BIN_OP_CASE(OpCode::EQUAL, left->GetValue() == right->GetValue())
    BIN_OP_CASE(OpCode::NEQUAL, left->GetValue() != right->GetValue())
    BIN_OP_CASE(OpCode::LESS, left->GetValue() < right->GetValue())
    BIN_OP_CASE(OpCode::GREATER, left->GetValue() > right->GetValue())
    BIN_OP_CASE(OpCode::LEQUAL, left->GetValue() <= right->GetValue())
    BIN_OP_CASE(OpCode::GEQUAL, left->GetValue() >= right->GetValue())
    BIN_OP_CASE(OpCode::AND, left->GetValue() && right->GetValue())
    BIN_OP_CASE(OpCode::OR, left->GetValue() || right->GetValue())
    default:
      throw std::logic_error("Unsupported binary operator for constant folding");
  }
}

/**
 * @brief FoldNodes for unary expression nodes
 */
static inline auto FoldNodes(UnaryExprNode *result, ExprNode *operand) -> void {
  CHECK(result != nullptr, "Result node cannot be null");
  CHECK(operand != nullptr, "Operand cannot be null");
  CHECK(operand->IsConst(), "Operand should be a constant expression to fold");
  // We can separately consider different operators
  switch (result->operator_) {
    UN_OP_CASE(OpCode::NEGATIVE, -operand->GetValue())
    UN_OP_CASE(OpCode::NOT, !operand->GetValue())
    UN_OP_CASE(OpCode::POSITIVE, +operand->GetValue())
    default:
      throw std::logic_error("Unsupported unary operator for constant folding");
  }
}

}  // namespace frontend::ast

#endif