#ifndef FRONTEND_AST_AST_DECL_H
#define FRONTEND_AST_AST_DECL_H

#include <memory>
#include <vector>
#include "ast/ast_base.h"
#include "ast/ast_expr.h"
#include "ast/ast_stmt.h"
#include "ast/astvisitor.h"
#include "token/location.h"
#include "type/type.h"

namespace frontend::ast {
/**
 * @brief Since the initializer nodes are storing ``ExprNode``,
 * we need to include ast_expr.h here to avoid incomplete type error.
 */
class ExprNode;
using ExprNodePtr = std::unique_ptr<ExprNode>;
class StmtNode;
using StmtNodePtr = std::unique_ptr<StmtNode>;

// We need to define variable declaration nodes to represent variable declarations in the AST.
// This includes nodes for single variable declarations, array declarations, and possibly initialization.
// Before implementing, we need to consider every type of variable declaration as well as how to handle ptr and integer.
class DeclNode : public StmtNode {
 public:
  explicit DeclNode(SourceRange range, TypePtr type, bool is_const = false)
      : StmtNode(range), type_(type), is_const_(is_const) {}
  ~DeclNode() override;
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
  ~Initializer() override;
  virtual auto IsConst() -> bool = 0;
  virtual auto IsScalar() const -> bool = 0;
};

using InitializerNodePtr = std::unique_ptr<Initializer>;

/**
 * @brief InitValNode representing a single value initializer
 * e.g., = 5;
 */
class InitValNode : public VisitAble<InitValNode, Initializer> {
 public:
  explicit InitValNode(SourceRange range, ExprNodePtr value)
      : VisitAble<InitValNode, Initializer>(range), value_(std::move(value)) {}
  ExprNodePtr value_;
  auto IsConst() -> bool override { return value_->IsConst(); }
  auto IsScalar() const -> bool override { return value_->GetType()->GetKind() != type::TypeKind::PTR; }
};

/**
 * @brief InitListNode representing a list initializer
 * e.g., = {1, 2, 3};
 * , = {1, {2, 3}, 4};
 * , = {{}, {}, 1};
 */
class InitListNode : public VisitAble<InitListNode, Initializer> {
 public:
  explicit InitListNode(SourceRange range, std::vector<InitializerNodePtr> init_values = {})
      : VisitAble<InitListNode, Initializer>(range), init_values_(std::move(init_values)) {}
  std::vector<InitializerNodePtr> init_values_;
  auto IsConst() -> bool override {
    for (const auto &init_value : init_values_) {
      if (!init_value->IsConst()) {
        return false;
      }
    }
    return true;
  }
  auto IsScalar() const -> bool override { return false; }
};

/**
 * @brief VarNode representing a single variable in a declaration
 * e.g., a; b[10];
 * Since we can have multiple variable definitions in one declaration statement
 * and the definitions are all the same in both global and local variable declarations
 * So we combine global and local variable definitions into one node class here.
 * @param ident_: the identifier of the variable
 * @param indices_: the indices of the array, if it is an array variable, otherwise it is empty
 * @param init_values_: the initializer of the variable, can be nullptr
 */
class VarDefNode : public VisitAble<VarDefNode, ASTNode> {
 public:
  explicit VarDefNode(SourceRange range, std::string ident, InitializerNodePtr init_values,
                      std::vector<ExprNodePtr> indices = {})
      : VisitAble<VarDefNode, ASTNode>(range),
        ident_(std::move(ident)),
        indices_(std::move(indices)),
        init_values_(std::move(init_values)) {}

  std::string ident_;
  // indexes for array
  std::vector<ExprNodePtr> indices_;  // can be empty
  InitializerNodePtr init_values_;    // can be empty
};

using VarDefNodePtr = std::unique_ptr<VarDefNode>;

// Then we can define declaration nodes.
/**
 * @brief GlobalVarDeclNode representing a global variable declaration
 * e.g., int a; float b[10];
 * it will only appear in the root node's children
 * @param defs_: the variable definitions in this declaration
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
 * @param defs_: the variable definitions in this declaration
 */
class LocalVarDeclNode : public VisitAble<LocalVarDeclNode, DeclNode> {
 public:
  explicit LocalVarDeclNode(SourceRange range, TypePtr type, std::vector<VarDefNodePtr> defs, bool is_const = false)
      : VisitAble<LocalVarDeclNode, DeclNode>(range, type, is_const), defs_(std::move(defs)) {}
  std::vector<VarDefNodePtr> defs_;
};

/**
 * @brief Function Parameter Node
 * Since it is different from variable declaration,
 * we define a separate node for function parameters
 * e.g., int a, float b[10]
 * @param param_name_: the name of the parameter
 */
class FuncFParamNode : public VisitAble<FuncFParamNode, ASTNode> {
 public:
  explicit FuncFParamNode(SourceRange range, TypePtr type, std::string param_name,
                          std::vector<ExprNodePtr> indices = {})
      : VisitAble<FuncFParamNode, ASTNode>(range),
        type_(type),
        param_name_(std::move(param_name)),
        indices_(std::move(indices)) {}
  TypePtr type_;
  std::string param_name_;
  std::vector<ExprNodePtr> indices_;  // can be empty
};

using FuncFParamNodePtr = std::unique_ptr<FuncFParamNode>;

/**
 * define function declaration node
 * @brief FunctionDeclNode representing a function declaration
 * @param func_name_: the name of the function
 * @param parameters_: the parameters of the function
 * @param body_: the body of the function, represented as a block statement
 */
class FuncDefNode : public VisitAble<FuncDefNode, ASTNode> {
 public:
  explicit FuncDefNode(SourceRange range, TypePtr return_type, std::string func_name, std::unique_ptr<StmtNode> body,
                       std::vector<FuncFParamNodePtr> parameters = {})
      : VisitAble<FuncDefNode, ASTNode>(range),
        return_type_(return_type),
        func_name_(std::move(func_name)),
        parameters_(std::move(parameters)),
        body_(std::move(body)) {}

  TypePtr return_type_;
  std::string func_name_;
  std::vector<FuncFParamNodePtr> parameters_;
  std::unique_ptr<StmtNode> body_;  // function body, represented as a block statement
};
}  // namespace frontend::ast
#endif