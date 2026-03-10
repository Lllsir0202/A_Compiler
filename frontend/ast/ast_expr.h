#ifndef FRONTEND_AST_AST_EXPR_H
#define FRONTEND_AST_AST_EXPR_H

#include <vector>
#include "ast/ast_base.h"
#include "ast/ast_utils.h"
#include "ast/astvisitor.h"
#include "token/location.h"
namespace frontend::ast {
/**
 * @brief Define parent class for all expression nodes
 * Since only expression nodes can be constant
 * and only expression nodes need to be considered as different types,
 * we can define a parent class for all expression nodes here.
 * And member variables' meanings are as follows:
 * @param is_const_: whether the expression is a constant expression
 * @param type_: the type of the expression (int, float, etc.),
 * especially we need to distinguish between int and ptr
 * @method: GetType(): get the type of the expression
 * @method: IsConst(): whether the expression is a constant expression
 * @method: SetType(TypePtr type): set the type of the expression,
 * used in semantic analysis phase to decide the type of the expression
 * @method: SetConst(bool is_const): set whether the expression is a constant expression
 */
class ExprNode : public ASTNode {
 public:
  explicit ExprNode(SourceRange range, TypePtr type, bool is_const = false)
      : ASTNode(range), type_(type), is_const_(is_const) {}
  ~ExprNode() override;
  auto IsConst() const -> bool { return is_const_; }
  auto SetConst(bool is_const) -> void { is_const_ = is_const; }
  auto GetType() const -> TypePtr { return type_; }
  auto SetType(TypePtr type) -> void { type_ = type; }
  // Since the expression can be calculated if it is const
  // we need to add a member variable to store the value
  // TODO(loki): add a struct

 private:
  TypePtr type_;
  bool is_const_;
};

using ExprNodePtr = std::unique_ptr<ExprNode>;

/**
 * @brief BinaryExp represents binary expressions, including all expressions with two operands, such as AddExp, RelExp,
 * EqExp, etc. We can use this class to represent all binary expressions, and use operator_ to distinguish between
 * different binary operators.
 * @param operator_: the operator of the binary expression, such as +, -, *, /, etc.
 * @param left_: the left operand of the binary expression
 * @param right_: the right operand of the binary expression
 * @note: type of BinaryExp is determined by the type of its operands and the operator
 */
class BinaryExprNode : public VisitAble<BinaryExprNode, ExprNode> {
 public:
  explicit BinaryExprNode(SourceRange range, TypePtr type, OpCode op, ExprNodePtr left, ExprNodePtr right,
                          bool is_const = false)
      : VisitAble<BinaryExprNode, ExprNode>(range, type, is_const),
        operator_(op),
        left_(std::move(left)),
        right_(std::move(right)) {}

  OpCode operator_;
  ExprNodePtr left_;
  ExprNodePtr right_;
};

/**
 * @brief UnaryExp represents all unary expressions, including all expressions with one operand, such as +, -, !
 * @param operator_: the operator of the unary expression, such as +, -, !, etc.
 * @param operand_: the operand of the unary expression
 * @note: type of UnaryExp is determined by the type of its operand
 */
class UnaryExprNode : public VisitAble<UnaryExprNode, ExprNode> {
 public:
  explicit UnaryExprNode(SourceRange range, TypePtr type, OpCode op, ExprNodePtr operand, bool is_const = false)
      : VisitAble<UnaryExprNode, ExprNode>(range, type, is_const), operator_(op), operand_(std::move(operand)) {}
  OpCode operator_;
  ExprNodePtr operand_;
};

/**
 * @brief FuncCallExp -> Ident ( [FuncRParams] )
 * Initially it is in UnaryExp, but we separate it for clarity.
 * Because we don't want unaryexpr to include to many things.
 * @note: type here is the return type of the function
 */
class FuncCallExprNode : public VisitAble<FuncCallExprNode, ExprNode> {
 public:
  explicit FuncCallExprNode(SourceRange range, TypePtr type, std::string func_name, std::vector<ExprNodePtr> params,
                            bool is_const = false)
      : VisitAble<FuncCallExprNode, ExprNode>(range, type, is_const),
        func_name_(std::move(func_name)),
        params_(std::move(params)) {}
  explicit FuncCallExprNode(SourceRange range, TypePtr type, std::string func_name, bool is_const = false)
      : VisitAble<FuncCallExprNode, ExprNode>(range, type, is_const), func_name_(std::move(func_name)) {}

  std::string func_name_;
  std::vector<ExprNodePtr> params_;
};

/**
 * @brief LVal represents the format like Ident { [Exp] }
 * @param ident_: the identifier of the variable
 * @param indices_: the indices of the array, if it is an array variable, otherwise it is empty
 * @note: type of LVal is determined by the type of the variable
 */
class LValExprNode : public VisitAble<LValExprNode, ExprNode> {
 public:
  explicit LValExprNode(SourceRange range, TypePtr type, std::string ident, std::vector<ExprNodePtr> indices,
                        bool is_const = false)
      : VisitAble<LValExprNode, ExprNode>(range, type, is_const),
        ident_(std::move(ident)),
        indices_(std::move(indices)) {
    is_array_ = true;
  }
  explicit LValExprNode(SourceRange range, TypePtr type, std::string ident, bool is_const = false)
      : VisitAble<LValExprNode, ExprNode>(range, type, is_const), ident_(std::move(ident)) {
    is_array_ = false;
  }

  std::string ident_;
  std::vector<ExprNodePtr> indices_;
  auto IsArray() const -> bool { return is_array_; }

 private:
  bool is_array_;
};

/**
 * @brief Number node representing a numeric literal
 * @note Initially in parser, only these two types are supported: int and float
 * and we will extend them later in semantic analysis phase
 */

/**
 * @brief IntLiteralExprNode represents an integer literal
 */
class IntLiteralExprNode : public VisitAble<IntLiteralExprNode, ExprNode> {
 public:
  explicit IntLiteralExprNode(SourceRange range, TypePtr type, int value)
      : VisitAble<IntLiteralExprNode, ExprNode>(range, type, true), value_(value) {}
  int value_;
};

/**
 * @brief FloatLiteralExprNode represents a floating-point literal
 */

class FloatLiteralExprNode : public VisitAble<FloatLiteralExprNode, ExprNode> {
 public:
  explicit FloatLiteralExprNode(SourceRange range, TypePtr type, float value)
      : VisitAble<FloatLiteralExprNode, ExprNode>(range, type, true), value_(value) {}
  float value_;
};

}  // namespace frontend::ast

#endif