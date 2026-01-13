#ifndef FRONTEND_AST_AST_STMT_H
#define FRONTEND_AST_AST_STMT_H

#include "ast/astvisitor.h"
namespace frontend::ast {
// define parent class for all statement nodes
class StmtNode : public VisitAble<StmtNode> {
 public:
  explicit StmtNode(int line, int column) : VisitAble<StmtNode>(line, column) {}
  ~StmtNode() override = default;
};

}  // namespace frontend::ast
#endif