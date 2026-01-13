#ifndef FRONTEND_AST_AST_STMT_H
#define FRONTEND_AST_AST_STMT_H

#include <vector>
#include "ast/astvisitor.h"
namespace frontend::ast {
// define parent class for all statement nodes
class StmtNode : public ASTNode {
 public:
  explicit StmtNode(SourceRange range) : ASTNode(range) {}
  ~StmtNode() override = default;
};

/**
 * @brief BlockStmtNode representing a block statement
 * Block -> '{' {BlockItem} '}'
 */
class BlockStmtNode : public VisitAble<BlockStmtNode, StmtNode> {
 public:
  explicit BlockStmtNode(SourceRange range, std::vector<ASTNodePtr> block_items)
      : VisitAble<BlockStmtNode, StmtNode>(range), block_items_(std::move(block_items)) {}
  std::vector<ASTNodePtr> block_items_;
};

/**
 * @brief AssignStmtNode representing an assignment statement
 * AssignStmt -> LVal '=' Exp ';'
 */
class AssignStmtNode : public VisitAble<AssignStmtNode, StmtNode> {
 public:
  explicit AssignStmtNode(SourceRange range, ASTNodePtr lval, ASTNodePtr expr)
      : VisitAble<AssignStmtNode, StmtNode>(range), lval_(std::move(lval)), expr_(std::move(expr)) {}
  ASTNodePtr lval_;
  ASTNodePtr expr_;
};

/**
 * @brief CommaStmtNode representing a comma-separated statement
 * CommaStmt -> [Exp] ';'
 */
class CommaStmtNode : public VisitAble<CommaStmtNode, StmtNode> {
 public:
  explicit CommaStmtNode(SourceRange range, ASTNodePtr expr = nullptr)
      : VisitAble<CommaStmtNode, StmtNode>(range), expr_(std::move(expr)) {}
  ASTNodePtr expr_;  // can be nullptr
};

/**
 * @brief IfStmtNode representing an if statement
 * IfStmt -> 'if' '(' Exp ')' Stmt [ 'else' Stmt ]
 */
class IfStmtNode : public VisitAble<IfStmtNode, StmtNode> {
 public:
  explicit IfStmtNode(SourceRange range, ASTNodePtr condition, ASTNodePtr then_branch, ASTNodePtr else_branch = nullptr)
      : VisitAble<IfStmtNode, StmtNode>(range),
        condition_(std::move(condition)),
        then_branch_(std::move(then_branch)),
        else_branch_(std::move(else_branch)) {}
  ASTNodePtr condition_;
  ASTNodePtr then_branch_;
  ASTNodePtr else_branch_;  // can be nullptr
};

/**
 * @brief whileStmtNode representing a while statement
 * WhileStmt -> 'while' '(' Exp ')' Stmt
 */
class WhileStmtNode : public VisitAble<WhileStmtNode, StmtNode> {
 public:
  explicit WhileStmtNode(SourceRange range, ASTNodePtr condition, ASTNodePtr body)
      : VisitAble<WhileStmtNode, StmtNode>(range), condition_(std::move(condition)), body_(std::move(body)) {}
  ASTNodePtr condition_;
  ASTNodePtr body_;
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
  explicit ReturnStmtNode(SourceRange range, ASTNodePtr expr = nullptr)
      : VisitAble<ReturnStmtNode, StmtNode>(range), expr_(std::move(expr)) {}
  ASTNodePtr expr_;  // can be nullptr
};

}  // namespace frontend::ast
#endif