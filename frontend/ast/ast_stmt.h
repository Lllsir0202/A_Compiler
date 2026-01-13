#ifndef FRONTEND_AST_AST_STMT_H
#define FRONTEND_AST_AST_STMT_H

#include "ast/astvisitor.h"
#include "token/location.h"
namespace frontend::ast {
// define parent class for all statement nodes
class StmtNode : public VisitAble<StmtNode> {
 public:
  explicit StmtNode(SourceRange range) : VisitAble<StmtNode>(range) {}
  ~StmtNode() override = default;
};

}  // namespace frontend::ast
#endif