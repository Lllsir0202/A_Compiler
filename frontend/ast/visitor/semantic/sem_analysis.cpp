// #include "frontend/ast/visitor/semantic/sem_analysis.h"
// #include <stdexcept>
// #include "ast/ast_decl.h"
// #include "ast/ast_expr.h"

// namespace frontend::ast {
// // RootNode
// void SemanticAnalyser::Visit(RootNode *root) {
//   for (auto &child : root->SemanticGetchildren()) {
//     child->Accept(this);
//   }
// }

// // Following are the nodes which should not be accessed directly
// void SemanticAnalyser::Visit([[maybe_unused]] ExprNode *expr) {
//   throw std::logic_error("ExprNode is an abstract class and should not be visited directly");
// }

// void SemanticAnalyser::Visit([[maybe_unused]] StmtNode *stmt) {
//   throw std::logic_error("StmtNode is an abstract class and should not be visited directly");
// }

// void SemanticAnalyser::Visit([[maybe_unused]] DeclNode *decl) {
//   throw std::logic_error("DeclNode is an abstract class and should not be visited directly");
// }

// void SemanticAnalyser::Visit([[maybe_unused]] Initializer *initializer) {
//   throw std::logic_error("Initializer is an abstract class and should not be visited directly");
// }

// // Then begin with expression nodes
// void SemanticAnalyser::Visit(BinaryExprNode *binary_expr) {
//     //Start with visiting the left and right operands
//     binary_expr->left_->Accept(this);
//     binary_expr->right_->Accept(this);

//     // Then we need to check the types of the left and right operands and the operator

// }

// }  // namespace frontend::ast