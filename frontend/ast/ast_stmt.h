#ifndef FRONTEND_AST_AST_STMT_H
#define FRONTEND_AST_AST_STMT_H

#include <memory>
#include <vector>
#include "ast/astvisitor.h"
namespace frontend::ast {
// define parent class for all statement nodes
class StmtNode : public ASTNode {
 public:
  explicit StmtNode(SourceRange range) : ASTNode(range) {}
  ~StmtNode() override;
};

using StmtNodePtr = std::unique_ptr<StmtNode>;
class ExprNode;
using ExprNodePtr = std::unique_ptr<ExprNode>;

/**
 * @brief BlockStmtNode representing a block statement
 * Block -> '{' {BlockItem} '}'
 */
class BlockStmtNode : public VisitAble<BlockStmtNode, StmtNode> {
 public:
  explicit BlockStmtNode(SourceRange range, std::vector<StmtNodePtr> block_items)
      : VisitAble<BlockStmtNode, StmtNode>(range), block_items_(std::move(block_items)) {}
  std::vector<StmtNodePtr> block_items_;
};

/**
 * @brief AssignStmtNode representing an assignment statement
 * AssignStmt -> LVal '=' Exp ';'
 * @note: we use ExprNode here and will check whether it is a LVal in semantic analysis phase.
 */
class AssignStmtNode : public VisitAble<AssignStmtNode, StmtNode> {
 public:
  explicit AssignStmtNode(SourceRange range, std::unique_ptr<ExprNode> lval, ExprNodePtr expr)
      : VisitAble<AssignStmtNode, StmtNode>(range), lval_(std::move(lval)), expr_(std::move(expr)) {}
  std::unique_ptr<ExprNode> lval_;
  ExprNodePtr expr_;
};

/**
 * @brief ExprStmtNode representing a expression statement
 * ExprStmt -> [Exp] ';'
 */
class ExprStmtNode : public VisitAble<ExprStmtNode, StmtNode> {
 public:
  explicit ExprStmtNode(SourceRange range, ExprNodePtr expr = nullptr)
      : VisitAble<ExprStmtNode, StmtNode>(range), expr_(std::move(expr)) {}
  ExprNodePtr expr_;  // can be nullptr
};

/**
 * @brief IfStmtNode representing an if statement
 * IfStmt -> 'if' '(' Exp ')' Stmt [ 'else' Stmt ]
 */
class IfStmtNode : public VisitAble<IfStmtNode, StmtNode> {
 public:
  explicit IfStmtNode(SourceRange range, ExprNodePtr condition, StmtNodePtr then_branch,
                      StmtNodePtr else_branch = nullptr)
      : VisitAble<IfStmtNode, StmtNode>(range),
        condition_(std::move(condition)),
        then_branch_(std::move(then_branch)),
        else_branch_(std::move(else_branch)) {}
  ExprNodePtr condition_;
  StmtNodePtr then_branch_;
  StmtNodePtr else_branch_;  // can be nullptr
};

/**
 * @brief whileStmtNode representing a while statement
 * WhileStmt -> 'while' '(' Exp ')' Stmt
 */
class WhileStmtNode : public VisitAble<WhileStmtNode, StmtNode> {
 public:
  explicit WhileStmtNode(SourceRange range, ExprNodePtr condition, StmtNodePtr body)
      : VisitAble<WhileStmtNode, StmtNode>(range), condition_(std::move(condition)), body_(std::move(body)) {}
  ExprNodePtr condition_;
  StmtNodePtr body_;
};

/**
 * @brief BreakStmtNode representing a break statement
 * BreakStmt -> 'break' ';'
 */
class BreakStmtNode : public VisitAble<BreakStmtNode, StmtNode> {
 public:
  explicit BreakStmtNode(SourceRange range) : VisitAble<BreakStmtNode, StmtNode>(range) {}
};

/**
 * @brief ContinueStmtNode representing a continue statement
 * ContinueStmt -> 'continue' ';'
 */
class ContinueStmtNode : public VisitAble<ContinueStmtNode, StmtNode> {
 public:
  explicit ContinueStmtNode(SourceRange range) : VisitAble<ContinueStmtNode, StmtNode>(range) {}
};

/**
 * @brief ReturnStmtNode representing a return statement
 * ReturnStmt -> 'return' [Exp] ';'
 */
class ReturnStmtNode : public VisitAble<ReturnStmtNode, StmtNode> {
 public:
  explicit ReturnStmtNode(SourceRange range, ExprNodePtr expr = nullptr)
      : VisitAble<ReturnStmtNode, StmtNode>(range), expr_(std::move(expr)) {}
  ExprNodePtr expr_;  // can be nullptr
};

}  // namespace frontend::ast
#endif