/**
 * @brief This file defines the LowerHelper class,
 * which provides helper functions for lowering high-level MLIR operations
 * to base dialect.
 */
#ifndef MIDDLEEND_MLIR_LOWER_LOWERHELPER_H
#define MIDDLEEND_MLIR_LOWER_LOWERHELPER_H
#include "context/compiler_context.h"
#include "mlir/dialect/arithm/arithm.h"
#include "mlir/dialect/base/base.h"
#include "mlir/dialect/func/func.h"
#include "mlir/dialect/scf/scf.h"
#include "mlir/dialect/var/var.h"
#include "mlir/utils/operation/operation.h"
#include "mlir/utils/struct/block.h"
#include "mlir/utils/struct/region.h"

namespace middleend::mlir::lower {
class LowerHelper {
 public:
  explicit LowerHelper(context::CompilerContext *compiler_context) : compiler_context_(compiler_context) {}
  ~LowerHelper() = default;

  /**
   * @brief Dispatch the lowering of different operations
   * since we cannot directly dispatch the lowering of different operations,
   * and via virtual functions or visitor pattern would be too heavy and coupled
   * for the dialects.
   */
  auto LowerDispatch(Operation *op) -> void;

  // Provide interface to set and get insert point
  auto SetInsertPoint(mlir::Block *block) -> void {
    if (block == nullptr) {
      compiler_context_->ReportError("Insert point block is nullptr");
      return;
    }
    current_block_ = block;
  }
  auto GetInsertPoint() -> mlir::Block * { return current_block_; }

  // Provide interface to set and get current region, which is used for control flow operations
  auto SetCurrentRegion(mlir::Region *region) -> void { current_region_ = region; }
  auto GetCurrentRegion() -> mlir::Region * {
    if (current_region_ == nullptr) {
      compiler_context_->ReportError("Current region is nullptr");
    }
    return current_region_;
  }

 private:
  // Detailed lowering functions for different operations
  // TODO(loki) : implement dispatching and lowering for different operations.
  /**
   * @brief lower arithmetic operations
   */
  // Binary operations
  auto LowerAddOp(middleend::mlir::arithm::AddOp *add_op) -> void;
  auto LowerSubOp(middleend::mlir::arithm::SubOp *sub_op) -> void;
  auto LowerMulOp(middleend::mlir::arithm::MulOp *mul_op) -> void;
  auto LowerDivOp(middleend::mlir::arithm::DivOp *div_op) -> void;
  auto LowerModOp(middleend::mlir::arithm::ModOp *mod_op) -> void;
  auto LowerEqualOp(middleend::mlir::arithm::EqualOp *equal_op) -> void;
  auto LowerNotEqualOp(middleend::mlir::arithm::NotEqualOp *ne_op) -> void;
  auto LowerLessOp(middleend::mlir::arithm::LessOp *less_op) -> void;
  auto LowerLessEqualOp(middleend::mlir::arithm::LessEqualOp *le_op) -> void;
  auto LowerGreaterOp(middleend::mlir::arithm::GreaterOp *greater_op) -> void;
  auto LowerGreaterEqualOp(middleend::mlir::arithm::GreaterEqualOp *ge_op) -> void;
  // Unary operations
  auto LowerNegOp(middleend::mlir::arithm::NegOp *neg_op) -> void;
  auto LowerNotOp(middleend::mlir::arithm::NotOp *not_op) -> void;
  auto LowerConstOp(middleend::mlir::arithm::ConstOp *const_op) -> void;
  auto LowerCastOp(middleend::mlir::arithm::CastOp *cast_op) -> void;

  // Copy operation(de facto not lowered to base dialect,
  // but we can still provide a lowering function currently)
  auto LowerCopyOp(middleend::mlir::arithm::CopyOp *copy_op) -> void;

  /**
   * @brief Function-about operations(de facto not lowered to base dialect,
   * but we can still provide a lowering function currently)
   */
  auto LowerFuncDefOp(middleend::mlir::func::FuncDefOp *func_def_op) -> void;
  auto LowerFuncCallOp(middleend::mlir::func::FuncCallOp *func_call_op) -> void;
  auto LowerReturnOp(middleend::mlir::func::ReturnOp *return_op) -> void;

  /**
   * @brief Variable-related operations(de facto not lowered to base dialect,
   * but we can still provide a lowering function currently)
   */
  auto LowerVarDefOp(middleend::mlir::var::VarDefOp *var_def_op) -> void;
  auto LowerVarGetPtrOp(middleend::mlir::var::VarGetPtrOp *var_get_ptr_op) -> void;
  auto LowerVarGetValOp(middleend::mlir::var::VarGetValOp *var_get_val_op) -> void;
  auto LowerVarSetOp(middleend::mlir::var::VarSetOp *var_set_op) -> void;

  /**
   * @brief Most importantly, control flow operations, which are the most complex ones to lower.
   */
  auto LowerIfOp(middleend::mlir::scf::IfOp *if_op) -> void;
  auto LowerWhileOp(middleend::mlir::scf::WhileOp *while_op) -> void;
  auto LowerYieldOp(middleend::mlir::scf::YieldOp *yield_op) -> void;
  auto LowerBreakOp(middleend::mlir::scf::BreakOp *break_op) -> void;
  auto LowerContinueOp(middleend::mlir::scf::ContinueOp *continue_op) -> void;
  auto LowerConditionOp(middleend::mlir::scf::ConditionOp *condition_op) -> void;
  auto LowerInitsOp(middleend::mlir::scf::InitsOp *inits_op) -> void;

  // Get Block id
  // Allow default block prefix
  auto SetUniqueBlockID(mlir::Block *block, const std::string &prefix = "") -> void {
    block->block_name_ = prefix;
    block->block_id_ = block_id_counter_++;
  }

  auto ResetBlockID() -> void { block_id_counter_ = 0; }

  // Provide interface to set and get current function definition operation, which is used for return operation to find
  // the entry block to insert the return operation
  auto SetCurrentFuncDefOp(mlir::base::FuncDefOp *func_def_op) -> void { current_func_def_op_ = func_def_op; }
  auto GetCurrentFuncDefOp() -> mlir::base::FuncDefOp * {
    if (current_func_def_op_ == nullptr) {
      compiler_context_->ReportError("Current function definition operation is nullptr");
    }
    return current_func_def_op_;
  }

 private:
  context::CompilerContext *compiler_context_;
  mlir::Block *current_block_;
  mlir::Region *current_region_;
  mlir::base::FuncDefOp *current_func_def_op_;
  size_t block_id_counter_ = 0;  // Counter for generating unique block names
};
}  // namespace middleend::mlir::lower

#endif