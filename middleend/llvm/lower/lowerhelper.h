#ifndef MIDDLEEND_LLVM_LOWER_LOWERHELPER_H
#define MIDDLEEND_LLVM_LOWER_LOWERHELPER_H
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/base/base.h"
#include "middleend/mlir/dialect/func/return.h"
#include "middleend/mlir/dialect/var/var.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "mlir/dialect/var/set.h"
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

  /**
   * @brief Lowering arithmetic operations
   */
  // Binary operations
  // Int
  auto LowerAddOp(mlir::base::AddOp *op) -> void;
  auto LowerSubOp(mlir::base::SubOp *op) -> void;
  auto LowerMulOp(mlir::base::MulOp *op) -> void;
  auto LowerDivOp(mlir::base::DivOp *op) -> void;
  auto LowerModOp(mlir::base::ModOp *op) -> void;
  // Float
  auto LowerAddFOp(mlir::base::AddFOp *op) -> void;
  auto LowerSubFOp(mlir::base::SubFOp *op) -> void;
  auto LowerMulFOp(mlir::base::MulFOp *op) -> void;
  auto LowerDivFOp(mlir::base::DivFOp *op) -> void;
  // Comparison operations
  // Int
  auto LowerICmpOp(mlir::Operation *op) -> void;
  auto LowerFCmpOp(mlir::Operation *op) -> void;

  // Unary operations
  auto LowerNegOp(mlir::base::NegOp *op) -> void;
  auto LowerNegFOp(mlir::base::NegFOp *op) -> void;
  auto LowerNotOp(mlir::base::NotOp *op) -> void;
  auto LowerIntLiteralOp(mlir::base::ConstIntOp *op) -> void;
  auto LowerFloatLiteralOp(mlir::base::ConstFloatOp *op) -> void;

  // Cast operations
  auto LowerInt2FloatOp(mlir::base::Int2FloatOp *op) -> void;
  auto LowerFloat2IntOp(mlir::base::Float2IntOp *op) -> void;
  auto LowerInt2BoolOp(mlir::base::Int2BoolOp *op) -> void;
  auto LowerFloat2BoolOp(mlir::base::Float2BoolOp *op) -> void;
  auto LowerBool2IntOp(mlir::base::Bool2IntOp *op) -> void;
  auto LowerBool2FloatOp(mlir::base::Bool2FloatOp *op) -> void;

  // Copy operation,
  // which is used for value forwarding,
  // and will be optimized away in the future
  auto LowerCopyOp(mlir::base::CopyOp *op) -> void;

  /**
   * @brief Function-related operations
   */
  auto LowerFuncDefOp(mlir::base::FuncDefOp *func_def) -> void;
  auto LowerFuncCallOp(mlir::base::FuncCallOp *func_call) -> void;
  auto LowerReturnOp(mlir::func::ReturnOp *return_op) -> void;

  /**
   * @brief Branch-related operations
   */
  auto LowerBrOp(mlir::base::BrOp *br_op) -> void;
  auto LowerCondBrOp(mlir::base::CondBrOp *cond_br_op) -> void;
  auto LowerPhiOp(mlir::base::PhiOp *phi_op) -> void;

  /**
   * @brief Memory-related operations
   */
  auto LowerVarDefOp(middleend::mlir::var::VarDefOp *var_def_op) -> void;
  auto LowerVarGetPtrOp(middleend::mlir::var::VarGetPtrOp *var_get_ptr_op) -> void;
  auto LowerGlobalVarDefOp(middleend::mlir::var::GlobalVarDefOp *global_var_def_op) -> void;
  auto LowerVarGetValOp(middleend::mlir::var::VarGetValOp *var_get_val_op) -> void;
  auto LowerVarSetOp(middleend::mlir::var::VarSetOp *var_set_val_op) -> void;
};

}  // namespace middleend::llvm::lower

#endif