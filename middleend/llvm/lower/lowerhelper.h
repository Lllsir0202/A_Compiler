#ifndef MIDDLEEND_LLVM_LOWER_LOWERHELPER_H
#define MIDDLEEND_LLVM_LOWER_LOWERHELPER_H
#include <queue>
#include "context/compiler_context.h"
#include "llvm/inst/pseudo/pseudo.h"
#include "middleend/llvm/inst/branch/phi.h"
#include "middleend/mlir/dialect/base/base.h"
#include "middleend/mlir/dialect/base/phi.h"
#include "middleend/mlir/dialect/func/return.h"
#include "middleend/mlir/dialect/var/set.h"
#include "middleend/mlir/dialect/var/var.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/pass/analysis/cfg/mlircfg.h"
namespace middleend::llvm::lower {
class LowerHelper {
 public:
  explicit LowerHelper(context::CompilerContext *compiler_context) : compiler_context_(compiler_context) {
    auto cfg_pass = compiler_context_->GetPassContext()->GetSpecPass<pass::analysis::CFGPass>("CFGPass");
    if (cfg_pass == nullptr) {
      throw std::logic_error("CFG pass is not registered in the pass context");
    }
    auto func_cfgs = cfg_pass->Get();
    for (auto *func_cfg : func_cfgs) {
      if (auto mlir_func_cfg = dynamic_cast<pass::analysis::MLIRFunctionCFG *>(func_cfg)) {
        auto func_def_op = mlir_func_cfg->GetFuncOp();
        func_cfgs_[func_def_op] = func_cfg;
      } else {
        throw std::logic_error("Function CFG is not of type MLIRFunctionCFG");
      }
    }
  }
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
  auto LowerWideMulModOp(mlir::base::WideMulModOp *op) -> void;
  auto LowerShlOp(mlir::base::ShlOp *op) -> void;
  auto LowerLShrOp(mlir::base::LShrOp *op) -> void;
  auto LowerAShrOp(mlir::base::AShrOp *op) -> void;
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
  auto LowerInt2LongOp(mlir::base::Int2LongOp *op) -> void;
  auto LowerLong2IntOp(mlir::base::Long2IntOp *op) -> void;

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

  std::queue<mlir::base::PhiOp *> pending_phi_ops_;
  std::queue<llvm::inst::PhiInst *> pending_phi_insts_;
  std::queue<llvm::inst::CopyInst *> pending_copy_insts_;

  // Map from op to the corresponding function CFG
  std::unordered_map<mlir::base::FuncDefOp *, pass::analysis::FunctionCFG *> func_cfgs_;
};

}  // namespace middleend::llvm::lower

#endif
