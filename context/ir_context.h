#ifndef CONTEXT_IR_CONTEXT_H
#define CONTEXT_IR_CONTEXT_H

#include "mlir_context.h"
#include "type_context.h"
namespace context {
/**
 * @brief IRContext is the context for the intermediate representation
 * it will provide common interface for all sorts of IR contexts
 * and it will be used as the resource manager for IR
 */
class IRContext {
 public:
  explicit IRContext(TypeContext *type_context) { mlir_context_ = new MLIRContext(type_context); }
  ~IRContext() { delete mlir_context_; }
  auto GetMLIRContext() -> MLIRContext * { return mlir_context_; }

 private:
  MLIRContext *mlir_context_;
};

}  // namespace context

#endif