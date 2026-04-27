#include "utils/flatten/mlirinit.h"
#include <stdexcept>
#include "ast/ast_decl.h"
#include "flatten/initializer.h"

namespace initializer {
/**
 * @brief Over here, we set nullptr to indicate the default initialzier
 * and the ExprNode* will be stored in the MLIRInitializer, which will be used for
 * get the result of which operation generate the result.
 */
auto HandleInIR(frontend::ast::Initializer *init_node) -> initializer::InitializerPtr {
  auto mlir_init = std::make_unique<MLIRInitializer>();
  if (init_node == nullptr) {
    // means there is default initializer
    mlir_init->SetValue(nullptr);
    return mlir_init;
  }
  if (init_node->IsScalar()) {
    auto scalar_init = static_cast<frontend::ast::InitValNode *>(init_node);
    // Since it is a scalar initializer, we can get the value
    auto &value = scalar_init->value_;
    mlir_init->SetValue(value.get());
  } else {
    // Not scalar which is not possible
    throw std::logic_error("Non-scalar initializer cannot be handled in ir expression evaluation");
  }
  return mlir_init;
}

}  // namespace initializer