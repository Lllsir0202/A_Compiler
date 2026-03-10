#include "ast/visitor/printer/astprinter.h"
#include <stdexcept>

namespace frontend::ast {
// RootNode
void ASTPrinter::Visit(const RootNode *root) {
  PrintIndent();
  *out_ << "RootNode\n";
  indent_level_++;
  for (const auto &child : root->GetChildren()) {
    child->Accept(this);
  }
  indent_level_--;
}
// Expression nodes
void ASTPrinter::Visit(const BinaryExprNode *binary_expr) {
  PrintIndent();
  *out_ << "BinaryExprNode: " << OpCodeToString(binary_expr->operator_);
  *out_ << "  --Type: " << binary_expr->GetType()->ToString() << "\n";
  PrintIndent();
  *out_ << "Left:\n";
  indent_level_++;
  binary_expr->left_->Accept(this);
  indent_level_--;
  PrintIndent();
  *out_ << "Right:\n";
  indent_level_++;
  binary_expr->right_->Accept(this);
  indent_level_--;
}
void ASTPrinter::Visit(const UnaryExprNode *unary_expr) {
  PrintIndent();
  *out_ << "UnaryExprNode: " << OpCodeToString(unary_expr->operator_);
  *out_ << "  --Type: " << unary_expr->GetType()->ToString() << "\n";
  PrintIndent();
  *out_ << "Operand:\n";
  indent_level_++;
  unary_expr->operand_->Accept(this);
  indent_level_--;
}
void ASTPrinter::Visit(const FuncCallExprNode *func_call) {
  PrintIndent();
  *out_ << "FuncCallExprNode: " << func_call->func_name_;
  *out_ << "  --Type: " << func_call->GetType()->ToString() << "\n";
  if (!func_call->params_.empty()) {
    PrintIndent();
    *out_ << "Parameters:\n";
    indent_level_++;
    for (const auto &param : func_call->params_) {
      param->Accept(this);
    }
    indent_level_--;
  } else {
    PrintIndent();
    *out_ << "Parameters: None\n";
  }
}
void ASTPrinter::Visit(const LValExprNode *lval_expr) {
  PrintIndent();
  *out_ << "LValExprNode: " << lval_expr->ident_;
  *out_ << "  --Type: " << lval_expr->GetType()->ToString() << "\n";
  indent_level_++;
  if (!lval_expr->indices_.empty()) {
    PrintIndent();
    *out_ << "Indices:\n";
    indent_level_++;
    for (const auto &index : lval_expr->indices_) {
      index->Accept(this);
    }
    indent_level_--;
  }
  indent_level_--;
}
void ASTPrinter::Visit(const IntLiteralExprNode *int_literal) {
  PrintIndent();
  *out_ << "IntLiteralExprNode: " << int_literal->value_ << "\n";
}
void ASTPrinter::Visit(const FloatLiteralExprNode *float_literal) {
  PrintIndent();
  *out_ << "FloatLiteralExprNode: " << float_literal->value_ << "\n";
}
// Statement nodes
void ASTPrinter::Visit(const BlockStmtNode *block_stmt) {
  PrintIndent();
  *out_ << "BlockStmtNode\n";
  indent_level_++;
  for (const auto &stmt : block_stmt->block_items_) {
    stmt->Accept(this);
  }
  indent_level_--;
}
void ASTPrinter::Visit(const AssignStmtNode *assign_stmt) {
  PrintIndent();
  *out_ << "AssignStmtNode\n";
  indent_level_++;
  PrintIndent();
  *out_ << "LVal:\n";
  indent_level_++;
  assign_stmt->lval_->Accept(this);
  indent_level_--;
  PrintIndent();
  *out_ << "Expr:\n";
  indent_level_++;
  assign_stmt->expr_->Accept(this);
  indent_level_--;
  indent_level_--;
}
void ASTPrinter::Visit(const ExprStmtNode *expr_stmt) {
  PrintIndent();
  *out_ << "ExprStmtNode\n";
  indent_level_++;
  if (expr_stmt->expr_) {
    expr_stmt->expr_->Accept(this);
  }
  indent_level_--;
}
void ASTPrinter::Visit(const IfStmtNode *if_stmt) {
  PrintIndent();
  *out_ << "IfStmtNode\n";
  indent_level_++;
  if_stmt->condition_->Accept(this);
  if_stmt->then_branch_->Accept(this);
  if (if_stmt->else_branch_) {
    if_stmt->else_branch_->Accept(this);
  }
  indent_level_--;
}
void ASTPrinter::Visit(const WhileStmtNode *while_stmt) {
  PrintIndent();
  *out_ << "WhileStmtNode\n";
  indent_level_++;
  while_stmt->condition_->Accept(this);
  while_stmt->body_->Accept(this);
  indent_level_--;
}
void ASTPrinter::Visit([[maybe_unused]] const BreakStmtNode *break_stmt) {
  PrintIndent();
  *out_ << "BreakStmtNode\n";
}
void ASTPrinter::Visit([[maybe_unused]] const ContinueStmtNode *continue_stmt) {
  PrintIndent();
  *out_ << "ContinueStmtNode\n";
}
void ASTPrinter::Visit(const ReturnStmtNode *return_stmt) {
  PrintIndent();
  *out_ << "ReturnStmtNode\n";
  indent_level_++;
  if (return_stmt->expr_) {
    return_stmt->expr_->Accept(this);
  }
  indent_level_--;
}
// Variable nodes includes decl and initializer
void ASTPrinter::Visit(const VarDefNode *var_def) {
  PrintIndent();
  *out_ << "VarDefNode: " << var_def->ident_ << "\n";
  indent_level_++;
  if (!var_def->indices_.empty()) {
    PrintIndent();
    *out_ << "Indices:\n";
    indent_level_++;
    for (const auto &index : var_def->indices_) {
      index->Accept(this);
    }
    indent_level_--;
  }
  if (var_def->init_values_) {
    var_def->init_values_->Accept(this);
  }
  indent_level_--;
}
void ASTPrinter::Visit(const GlobalVarDeclNode *global_var_decl) {
  PrintIndent();
  if (global_var_decl->IsConst()) {
    *out_ << "Const GlobalVarDeclNode";
  } else {
    *out_ << "GlobalVarDeclNode";
  }
  *out_ << "  --Type: " << global_var_decl->GetType()->ToString() << "\n";
  indent_level_++;
  for (const auto &def : global_var_decl->defs_) {
    def->Accept(this);
  }
  indent_level_--;
}
void ASTPrinter::Visit(const LocalVarDeclNode *local_var_decl) {
  PrintIndent();
  if (local_var_decl->IsConst()) {
    *out_ << "Const LocalVarDeclNode";
  } else {
    *out_ << "LocalVarDeclNode";
  }
  *out_ << "  --Type: " << local_var_decl->GetType()->ToString() << "\n";
  indent_level_++;
  for (const auto &def : local_var_decl->defs_) {
    def->Accept(this);
  }
  indent_level_--;
}
void ASTPrinter::Visit(const InitListNode *init_list) {
  PrintIndent();
  *out_ << "InitListNode\n";
  indent_level_++;
  for (const auto &init_val : init_list->init_values_) {
    init_val->Accept(this);
  }
  indent_level_--;
}
void ASTPrinter::Visit(const InitValNode *init_val) {
  PrintIndent();
  *out_ << "InitValNode\n";
  indent_level_++;
  init_val->value_->Accept(this);
  indent_level_--;
}

// Abstract Classes
void ASTPrinter::Visit([[maybe_unused]] const Initializer *initializer) {
  throw std::logic_error("Initializer is an abstract class and should not be visited directly");
}
void ASTPrinter::Visit([[maybe_unused]] const DeclNode *decl_node) {
  throw std::logic_error("DeclNode is an abstract class and should not be visited directly");
}
void ASTPrinter::Visit([[maybe_unused]] const StmtNode *stmt_node) {
  throw std::logic_error("StmtNode is an abstract class and should not be visited directly");
}
void ASTPrinter::Visit([[maybe_unused]] const ExprNode *expr_node) {
  throw std::logic_error("ExprNode is an abstract class and should not be visited directly");
}

// Function nodes
void ASTPrinter::Visit(const FuncFParamNode *func_fparam) {
  PrintIndent();
  *out_ << "FuncFParamNode: " << func_fparam->param_name_ << "\n";
  indent_level_++;
  if (!func_fparam->indices_.empty()) {
    PrintIndent();
    *out_ << "Indices:\n";
    indent_level_++;
    for (const auto &index : func_fparam->indices_) {
      if (!index) {
        PrintIndent();
        *out_ << "Empty index\n";
        continue;
      }
      index->Accept(this);
    }
    indent_level_--;
  }
  indent_level_--;
}

void ASTPrinter::Visit(const FuncDefNode *func_def) {
  // Print function name
  {
    PrintIndent();
    *out_ << "FuncDefNode: " << func_def->func_name_ << "\n";
  }
  // Print return type
  {
    indent_level_++;
    PrintIndent();
    *out_ << "Return Type: " << func_def->return_type_->ToString() << "\n";
  }
  // Print parameters
  {
    if (!func_def->parameters_.empty()) {
      PrintIndent();
      *out_ << "Parameters:\n";
      indent_level_++;
      for (const auto &param : func_def->parameters_) {
        param->Accept(this);
      }
      indent_level_--;
    } else {
      PrintIndent();
      *out_ << "Parameters: None\n";
    }
  }
  // Print body
  {
    PrintIndent();
    *out_ << "Body:\n";
    indent_level_++;
    func_def->body_->Accept(this);
    indent_level_--;
  }
  indent_level_--;
}

void ASTPrinter::PrintIndent() const {
  for (int i = 0; i < indent_level_; i++) {
    *out_ << "  ";
  }
}

}  // namespace frontend::ast