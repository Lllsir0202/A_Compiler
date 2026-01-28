#ifndef FRONTEND_AST_AST_DECL_H
#define FRONTEND_AST_AST_DECL_H

#include <memory>
#include <vector>
#include "ast/ast_base.h"
#include "ast/astvisitor.h"

namespace frontend::ast {
/**
 * @brief Since the initializer nodes are storing ``ExprNode``,
 * we need to include ast_expr.h here to avoid incomplete type error.
 */
class ExprNode;
using ExprNodePtr = std::unique_ptr<ExprNode>;

// We need to define variable declaration nodes to represent variable declarations in the AST.
// This includes nodes for single variable declarations, array declarations, and possibly initialization.
// Before implementing, we need to consider every type of variable declaration as well as how to handle ptr and integer.
class DeclNode : public ASTNode {
 public:
  explicit DeclNode(SourceRange range, TypePtr type, bool is_const = false)
      : ASTNode(range), type_(type), is_const_(is_const) {}
  ~DeclNode() override = default;
  auto GetType() const -> TypePtr { return type_; }
  auto IsConst() const -> bool { return is_const_; }

 private:
  TypePtr type_;
  bool is_const_;
};

/**
 * Then we define initializer nodes
 * @brief InitializerNode representing an initializer
 * e.g., = 5; = {1, 2, 3};
 */
class Initializer : public ASTNode {
 public:
  explicit Initializer(SourceRange range) : ASTNode(range) {}
  ~Initializer() override = default;
};

/**
 * @brief InitValNode representing a single value initializer
 * e.g., = 5;
 */
class InitValNode : public VisitAble<InitValNode, Initializer> {
 public:
  explicit InitValNode(SourceRange range, ExprNodePtr value)
      : VisitAble<InitValNode, Initializer>(range), value_(std::move(value)) {}
  ExprNodePtr value_;
};

/**
 * @brief InitListNode representing a list initializer
 * e.g., = {1, 2, 3};
 */
class InitListNode : public VisitAble<InitListNode, Initializer> {
 public:
  explicit InitListNode(SourceRange range, std::vector<ExprNodePtr> init_values)
      : VisitAble<InitListNode, Initializer>(range), init_values_(std::move(init_values)) {}
  std::vector<ExprNodePtr> init_values_;
};

/**
 * @brief VarNode representing a single variable in a declaration
 * e.g., a; b[10];
 * Since we can have multiple variable definitions in one declaration statement
 * and the definitions are all the same in both global and local variable declarations
 * So we combine global and local variable definitions into one node class here.
 */
class VarDefNode : public VisitAble<VarDefNode, ASTNode> {
 public:
  explicit VarDefNode(SourceRange range, std::string ident, std::vector<ASTNodePtr> indices = {},
                      ASTNodePtr init_values = nullptr)
      : VisitAble<VarDefNode, ASTNode>(range),
        ident_(std::move(ident)),
        indices_(std::move(indices)),
        init_values_(std::move(init_values)) {}
  std::string ident_;
  // indexes for array
  std::vector<ASTNodePtr> indices_;  // can be empty
  ASTNodePtr init_values_;           // can be empty
};

using VarDefNodePtr = std::unique_ptr<VarDefNode>;

// Then we can define declaration nodes.
/**
 * @brief GlobalVarDeclNode representing a global variable declaration
 * e.g., int a; float b[10];
 * it will only appear in the root node's children
 */
class GlobalVarDeclNode : public VisitAble<GlobalVarDeclNode, DeclNode> {
 public:
  explicit GlobalVarDeclNode(SourceRange range, TypePtr type, std::vector<VarDefNodePtr> defs, bool is_const = false)
      : VisitAble<GlobalVarDeclNode, DeclNode>(range, type, is_const), defs_(std::move(defs)) {}
  std::vector<VarDefNodePtr> defs_;
};

/**
 * @brief LocalVarDeclNode representing a local variable declaration
 * e.g., int a; float b[10];
 * it will appear in block statements
 * const or not can be determined here
 * and we can have multiple local variable declarations in one statement
 */
class LocalVarDeclNode : public VisitAble<LocalVarDeclNode, DeclNode> {
 public:
  explicit LocalVarDeclNode(SourceRange range, TypePtr type, std::vector<VarDefNodePtr> defs, bool is_const = false)
      : VisitAble<LocalVarDeclNode, DeclNode>(range, type, is_const), defs_(std::move(defs)) {}
  std::vector<VarDefNodePtr> defs_;
};

/**
 * TODO(loki): define function declaration node
 * @brief FunctionDeclNode representing a function declaration
 */

}  // namespace frontend::ast
#endif