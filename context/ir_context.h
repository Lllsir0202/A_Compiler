#ifndef CONTEXT_IR_CONTEXT_H
#define CONTEXT_IR_CONTEXT_H

#include "context/llvmir_context.h"
#include "context/mlir_context.h"
#include "context/type_context.h"
namespace context {
/**
 * @brief IRContext is the context for the intermediate representation
 * it will provide common interface for all sorts of IR contexts
 * and it will be used as the resource manager for IR
 */
class IRContext {
 public:
  explicit IRContext(TypeContext *type_context) {
    mlir_context_ = new MLIRContext(type_context);
    llvmir_context_ = new LLVMIRContext(type_context);
  }
  ~IRContext() {
    delete mlir_context_;
    delete llvmir_context_;
  }
  auto GetMLIRContext() -> MLIRContext * { return mlir_context_; }
  auto GetLLVMIRContext() -> LLVMIRContext * { return llvmir_context_; }

 private:
  MLIRContext *mlir_context_;
  LLVMIRContext *llvmir_context_;
};

}  // namespace context

#endif