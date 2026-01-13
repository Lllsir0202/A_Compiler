#ifndef FRONTEND_AST_AST_EXPR_H
#define FRONTEND_AST_AST_EXPR_H

#include <vector>
#include "ast/ast_utils.h"
#include "ast/astvisitor.h"
#include "token/location.h"
namespace frontend::ast {
// Define parent class for all expression nodes
class ExprNode : public ASTNode {
 public:
  explicit ExprNode(SourceRange range, bool is_const = false) : ASTNode(range), is_const_(is_const) {}
  ~ExprNode() override = default;
  auto IsConst() const -> bool { return is_const_; }

 private:
  bool is_const_;
};

/**
 * @brief AddExp -> MulExp | AddExp {+ -} MulExp
 */
class AddExprNode : public VisitAble<AddExprNode, ExprNode> {
 public:
  explicit AddExprNode(SourceRange range, ASTNodePtr left, ASTNodePtr right, bool is_const = false)
      : VisitAble<AddExprNode, ExprNode>(range, is_const), left_(std::move(left)), right_(std::move(right)) {}

  OpCode operator_;
  ASTNodePtr left_;
  ASTNodePtr right_;
};

/**
 * @brief MulExp -> UnaryExp | MulExp {* / %} UnaryExp
 */
class MulExprNode : public VisitAble<MulExprNode, ExprNode> {
 public:
  explicit MulExprNode(SourceRange range, ASTNodePtr left, ASTNodePtr right, bool is_const = false)
      : VisitAble<MulExprNode, ExprNode>(range, is_const), left_(std::move(left)), right_(std::move(right)) {}
  OpCode operator_;
  ASTNodePtr left_;
  ASTNodePtr right_;
};

/**
 * @brief UnaryExp -> PrimaryExp | {+ - !} UnaryExp
 */
class UnaryExprNode : public VisitAble<UnaryExprNode, ExprNode> {
 public:
  explicit UnaryExprNode(SourceRange range, ASTNodePtr operand, bool is_const = false)
      : VisitAble<UnaryExprNode, ExprNode>(range, is_const), operand_(std::move(operand)) {}
  OpCode operator_;
  ASTNodePtr operand_;
};

/**
 * @brief PrimaryExp -> (Exp) | LVal | Number
 */
class PrimaryExprNode : public VisitAble<PrimaryExprNode, ExprNode> {
 public:
  explicit PrimaryExprNode(SourceRange range, ASTNodePtr value, bool is_const = false)
      : VisitAble<PrimaryExprNode, ExprNode>(range, is_const), value_(std::move(value)) {}

  ASTNodePtr value_;
};

/**
 * @brief LVal -> Ident { [Exp] }
 */
class LValExprNode : public VisitAble<LValExprNode, ExprNode> {
 public:
  explicit LValExprNode(SourceRange range, std::string ident, std::vector<ASTNodePtr> indices, bool is_const = false)
      : VisitAble<LValExprNode, ExprNode>(range, is_const), ident_(std::move(ident)), indices_(std::move(indices)) {}

  std::string ident_;
  std::vector<ASTNodePtr> indices_;
};

/**
 * @brief RelExp -> AddExp | RelExp {< <= > >=} AddExp
 */
class RelExprNode : public VisitAble<RelExprNode, ExprNode> {
 public:
  explicit RelExprNode(SourceRange range, ASTNodePtr left, ASTNodePtr right, bool is_const = false)
      : VisitAble<RelExprNode, ExprNode>(range, is_const), left_(std::move(left)), right_(std::move(right)) {}

  OpCode operator_;
  ASTNodePtr left_;
  ASTNodePtr right_;
};

/**
 * @brief EqExp -> RelExp | EqExp {== !=} RelExp
 */
class EqExprNode : public VisitAble<EqExprNode, ExprNode> {
 public:
  explicit EqExprNode(SourceRange range, ASTNodePtr left, ASTNodePtr right, bool is_const = false)
      : VisitAble<EqExprNode, ExprNode>(range, is_const), left_(std::move(left)), right_(std::move(right)) {}

  OpCode operator_;
  ASTNodePtr left_;
  ASTNodePtr right_;
};

/**
 * @brief LAndExp -> EqExp | LAndExp && EqExp
 */
class LAndExprNode : public VisitAble<LAndExprNode, ExprNode> {
 public:
  explicit LAndExprNode(SourceRange range, ASTNodePtr left, ASTNodePtr right, bool is_const = false)
      : VisitAble<LAndExprNode, ExprNode>(range, is_const), left_(std::move(left)), right_(std::move(right)) {}

  OpCode operator_ = OpCode::AND;
  ASTNodePtr left_;
  ASTNodePtr right_;
};

/**
 * @brief LOrExp -> LAndExp | LOrExp || LAndExp
 */
class LOrExprNode : public VisitAble<LOrExprNode, ExprNode> {
 public:
  explicit LOrExprNode(SourceRange range, ASTNodePtr left, ASTNodePtr right, bool is_const = false)
      : VisitAble<LOrExprNode, ExprNode>(range, is_const), left_(std::move(left)), right_(std::move(right)) {}

  OpCode operator_ = OpCode::OR;
  ASTNodePtr left_;
  ASTNodePtr right_;
};

/**
 * @brief Number node representing a numeric literal
 */

/**
 * @brief IntLiteralExprNode representing an integer literal
 */
class IntLiteralExprNode : public VisitAble<IntLiteralExprNode, ExprNode> {
 public:
  explicit IntLiteralExprNode(SourceRange range, int value)
      : VisitAble<IntLiteralExprNode, ExprNode>(range, true), value_(value) {}
  int value_;
};

/**
 * @brief FloatLiteralExprNode representing a floating-point literal
 */

class FloatLiteralExprNode : public VisitAble<FloatLiteralExprNode, ExprNode> {
 public:
  explicit FloatLiteralExprNode(SourceRange range, float value)
      : VisitAble<FloatLiteralExprNode, ExprNode>(range, true), value_(value) {}
  float value_;
};

}  // namespace frontend::ast

#endif