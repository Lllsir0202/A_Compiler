#ifndef FRONTEND_AST_AST_DECL_H
#define FRONTEND_AST_AST_DECL_H

#include <vector>
#include "ast/ast_base.h"
#include "ast/astvisitor.h"

namespace frontend::ast {

// TODO(loki): define variable declaration node
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
  explicit InitValNode(SourceRange range, ASTNodePtr value)
      : VisitAble<InitValNode, Initializer>(range), value_(std::move(value)) {}
  ASTNodePtr value_;
};

/**
 * @brief InitListNode representing a list initializer
 * e.g., = {1, 2, 3};
 */
class InitListNode : public VisitAble<InitListNode, Initializer> {
 public:
  explicit InitListNode(SourceRange range, std::vector<ASTNodePtr> init_values)
      : VisitAble<InitListNode, Initializer>(range), init_values_(std::move(init_values)) {}
  std::vector<ASTNodePtr> init_values_;
};

// Then we can define declaration nodes.
/**
 * @brief GlobalVarDeclNode representing a global variable declaration
 * e.g., int a; float b[10];
 * it will only appear in the root node's children
 */
class GlobalVarDeclNode : public VisitAble<GlobalVarDeclNode, DeclNode> {
 public:
  explicit GlobalVarDeclNode(SourceRange range, TypePtr type, std::vector<ASTNodePtr> defs, bool is_const = false)
      : VisitAble<GlobalVarDeclNode, DeclNode>(range, type, is_const), defs_(std::move(defs)) {}
  std::vector<ASTNodePtr> defs_;  // can be empty
};

/**
 * @brief Same as local var we need to add defnode for global var
 */
class GlobalVarDefNode : public VisitAble<GlobalVarDefNode, ASTNode> {
 public:
  explicit GlobalVarDefNode(SourceRange range, std::string ident, ASTNodePtr init_values = nullptr)
      : VisitAble<GlobalVarDefNode, ASTNode>(range), ident_(std::move(ident)), init_values_(std::move(init_values)) {}
  std::string ident_;
  ASTNodePtr init_values_;  // can be empty
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
  explicit LocalVarDeclNode(SourceRange range, TypePtr type, std::vector<ASTNodePtr> defs, bool is_const = false)
      : VisitAble<LocalVarDeclNode, DeclNode>(range, type, is_const), defs_(std::move(defs)) {}
  std::vector<ASTNodePtr> defs_;
};

/**
 * @brief VarNode representing a single variable in a declaration
 * e.g., a; b[10];
 */
class LocalVarDefNode : public VisitAble<LocalVarDefNode, ASTNode> {
 public:
  explicit LocalVarDefNode(SourceRange range, std::string ident, ASTNodePtr init_values = nullptr)
      : VisitAble<LocalVarDefNode, ASTNode>(range), ident_(std::move(ident)), init_values_(std::move(init_values)) {}
  std::string ident_;
  ASTNodePtr init_values_;  // can be empty
};

/**
* TODO(loki): define function declaration node
* @brief FunctionDeclNode representing a function declaration
*/

}  // namespace frontend::ast
#endif