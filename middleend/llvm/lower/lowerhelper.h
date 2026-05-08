#ifndef MIDDLEEND_LLVM_LOWER_LOWERHELPER_H
#define MIDDLEEND_LLVM_LOWER_LOWERHELPER_H
#include "context/compiler_context.h"
#include "mlir/dialect/base/funcdef.h"
#include "mlir/utils/operation/operation.h"
namespace middleend::llvm::lower {
class LowerHelper {
 public:
  explicit LowerHelper(context::CompilerContext *compiler_context) : compiler_context_(compiler_context) {}
  ~LowerHelper() = default;

  /**
   * @brief Dispatch the lowering operation in base dialect
   */
  auto LowerDispatch(mlir::Operation *op) -> void;

  // Since we have translated it into cfg form,
  // we can directly lower the basic blocks first,
  // so that all blocks will always be the same instance of the same block id
  auto LowerBasicBlocks(mlir::base::FuncDefOp *func_def) -> void;

 private:
  context::CompilerContext *compiler_context_;

  // Lower helper functions
  auto LowerFuncDef(mlir::base::FuncDefOp *func_def) -> void;
};

}  // namespace middleend::llvm::lower

#endif