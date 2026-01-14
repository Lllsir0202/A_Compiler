#ifndef FRONTEND_AST_AST_BASE_H
#define FRONTEND_AST_AST_BASE_H

#include <memory>
#include "token/location.h"
#include "utils/type/type.h"
namespace frontend::ast {
class ASTVisitor;

// We define basic AST Node here
class ASTNode {
 public:
  // override constructors
  [[maybe_unused]] ASTNode(int line, int column) : line_(line), column_(column) {}
  explicit ASTNode(frontend::SourceRange source_range)
      : line_(source_range.GetBegin().GetLine()), column_(source_range.GetBegin().GetColumn()) {}
  explicit ASTNode() : line_(0), column_(0) {}
  virtual ~ASTNode() = default;
  virtual void Accept(ASTVisitor *visitor) = 0;
  auto GetLine() const { return line_; }
  auto GetColumn() const { return column_; }

 private:
  int line_;
  int column_;
};

using ASTNodePtr = std::unique_ptr<ASTNode>;
using TypePtr = type::Type*;

}  // namespace frontend::ast

#endif