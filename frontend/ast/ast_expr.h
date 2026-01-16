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
 */
class ExprNode : public ASTNode {
 public:
  explicit ExprNode(SourceRange range, TypePtr type, bool is_const = false)
      : ASTNode(range), type_(type), is_const_(is_const) {}
  ~ExprNode() override = default;
  auto IsConst() const -> bool { return is_const_; }
  auto GetType() const -> TypePtr { return type_; }
  auto SetType(TypePtr type) -> void { type_ = type; }
  auto SetConst(bool is_const) -> void { is_const_ = is_const; }

 private:
  TypePtr type_;
  bool is_const_;
};

/**
 * @brief AddExp -> MulExp | AddExp {+ -} MulExp
 */
class AddExprNode : public VisitAble<AddExprNode, ExprNode> {
 public:
  explicit AddExprNode(SourceRange range, TypePtr type, ASTNodePtr left, ASTNodePtr right, bool is_const = false)
      : VisitAble<AddExprNode, ExprNode>(range, type, is_const), left_(std::move(left)), right_(std::move(right)) {}

  OpCode operator_;
  ASTNodePtr left_;
  ASTNodePtr right_;
};

/**
 * @brief MulExp -> UnaryExp | MulExp {* / %} UnaryExp
 */
class MulExprNode : public VisitAble<MulExprNode, ExprNode> {
 public:
  explicit MulExprNode(SourceRange range, TypePtr type, ASTNodePtr left, ASTNodePtr right, bool is_const = false)
      : VisitAble<MulExprNode, ExprNode>(range, type, is_const), left_(std::move(left)), right_(std::move(right)) {}
  OpCode operator_;
  ASTNodePtr left_;
  ASTNodePtr right_;
};

/**
 * @brief UnaryExp -> PrimaryExp | {+ - !} UnaryExp
 */
class UnaryExprNode : public VisitAble<UnaryExprNode, ExprNode> {
 public:
  explicit UnaryExprNode(SourceRange range, TypePtr type, ASTNodePtr operand, bool is_const = false)
      : VisitAble<UnaryExprNode, ExprNode>(range, type, is_const), operand_(std::move(operand)) {}
  OpCode operator_;
  ASTNodePtr operand_;
};

/**
 * @brief FuncCallExp -> Ident ( [FuncRParams] )
 * Initially it is in UnaryExp, but we separate it for clarity.
 * Because we don't want unaryexpr to include to many things.
 * @note: type here is the return type of the function
 */
class FuncCallExprNode : public VisitAble<FuncCallExprNode, ExprNode> {
 public:
  explicit FuncCallExprNode(SourceRange range, TypePtr type, std::string func_name, std::vector<ASTNodePtr> params,
                            bool is_const = false)
      : VisitAble<FuncCallExprNode, ExprNode>(range, type, is_const),
        func_name_(std::move(func_name)),
        params_(std::move(params)) {}

  std::string func_name_;
  std::vector<ASTNodePtr> params_;
};

/**
 * @brief PrimaryExp -> (Exp) | LVal | Number
 */
class PrimaryExprNode : public VisitAble<PrimaryExprNode, ExprNode> {
 public:
  explicit PrimaryExprNode(SourceRange range, TypePtr type, ASTNodePtr value, bool is_const = false)
      : VisitAble<PrimaryExprNode, ExprNode>(range, type, is_const), value_(std::move(value)) {}

  ASTNodePtr value_;
};

/**
 * @brief LVal -> Ident { [Exp] }
 */
class LValExprNode : public VisitAble<LValExprNode, ExprNode> {
 public:
  explicit LValExprNode(SourceRange range, TypePtr type, std::string ident, std::vector<ASTNodePtr> indices,
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
  std::vector<ASTNodePtr> indices_;
  auto IsArray() const -> bool { return is_array_; }

 private:
  bool is_array_;
};

/**
 * @brief RelExp -> AddExp | RelExp {< <= > >=} AddExp
 */
class RelExprNode : public VisitAble<RelExprNode, ExprNode> {
 public:
  explicit RelExprNode(SourceRange range, TypePtr type, ASTNodePtr left, ASTNodePtr right, bool is_const = false)
      : VisitAble<RelExprNode, ExprNode>(range, type, is_const), left_(std::move(left)), right_(std::move(right)) {}

  OpCode operator_;
  ASTNodePtr left_;
  ASTNodePtr right_;
};

/**
 * @brief EqExp -> RelExp | EqExp {== !=} RelExp
 */
class EqExprNode : public VisitAble<EqExprNode, ExprNode> {
 public:
  explicit EqExprNode(SourceRange range, TypePtr type, ASTNodePtr left, ASTNodePtr right, bool is_const = false)
      : VisitAble<EqExprNode, ExprNode>(range, type, is_const), left_(std::move(left)), right_(std::move(right)) {}

  OpCode operator_;
  ASTNodePtr left_;
  ASTNodePtr right_;
};

/**
 * @brief LAndExp -> EqExp | LAndExp && EqExp
 */
class LAndExprNode : public VisitAble<LAndExprNode, ExprNode> {
 public:
  explicit LAndExprNode(SourceRange range, TypePtr type, ASTNodePtr left, ASTNodePtr right, bool is_const = false)
      : VisitAble<LAndExprNode, ExprNode>(range, type, is_const), left_(std::move(left)), right_(std::move(right)) {}

  OpCode operator_ = OpCode::AND;
  ASTNodePtr left_;
  ASTNodePtr right_;
};

/**
 * @brief LOrExp -> LAndExp | LOrExp || LAndExp
 */
class LOrExprNode : public VisitAble<LOrExprNode, ExprNode> {
 public:
  explicit LOrExprNode(SourceRange range, TypePtr type, ASTNodePtr left, ASTNodePtr right, bool is_const = false)
      : VisitAble<LOrExprNode, ExprNode>(range, type, is_const), left_(std::move(left)), right_(std::move(right)) {}

  OpCode operator_ = OpCode::OR;
  ASTNodePtr left_;
  ASTNodePtr right_;
};

/**
 * @brief Number node representing a numeric literal
 * @note Initially in parser, only these two types are supported: int and float
 * and we will extend them later in semantic analysis phase
 */

/**
 * @brief IntLiteralExprNode representing an integer literal
 */
class IntLiteralExprNode : public VisitAble<IntLiteralExprNode, ExprNode> {
 public:
  explicit IntLiteralExprNode(SourceRange range, TypePtr type, int value)
      : VisitAble<IntLiteralExprNode, ExprNode>(range, type, true), value_(value) {}
  int value_;
};

/**
 * @brief FloatLiteralExprNode representing a floating-point literal
 */

class FloatLiteralExprNode : public VisitAble<FloatLiteralExprNode, ExprNode> {
 public:
  explicit FloatLiteralExprNode(SourceRange range, TypePtr type, float value)
      : VisitAble<FloatLiteralExprNode, ExprNode>(range, type, true), value_(value) {}
  float value_;
};

}  // namespace frontend::ast

#endif