#ifndef UTILS_FLATTEN_MLIRINIT_H
#define UTILS_FLATTEN_MLIRINIT_H
#include "ast/ast_decl.h"
#include "ast/ast_expr.h"
#include "mlir/utils/value/value.h"
#include "mlir_context.h"
#include "utils/flatten/initializer.h"

namespace initializer {
/**
 * @brief This class represents the initializers in the MLIR phase
 * @method: SetValue(frontend::ast::ExprNode *value) -> void:
 * set the value of the initializer, which is the result of the initializer
 * @method: GetValue() -> frontend::ast::ExprNode *: get the value of the initializer
 * @note: This class is used for representing the initializers in the MLIR phase
 * it shall only be called while the initializer is NOT all constant,
 * since the constant initializer has been evaluated in the AST phase.
 */
class MLIRInitializer : public Initializer {
 public:
  MLIRInitializer() = default;
  auto SetValue(frontend::ast::ExprNode *value) -> void { value_ = value; }
  auto GetValue() -> frontend::ast::ExprNode * { return value_; }
  auto IsScalar() const -> bool override { return true; }
  auto IsConst() const -> bool override { return false; }

 private:
  frontend::ast::ExprNode *value_ = nullptr;
};

auto HandleInIR(frontend::ast::Initializer *init_node) -> InitializerPtr;

}  // namespace initializer

#endif