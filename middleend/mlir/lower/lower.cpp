#include "middleend/mlir/lower/lower.h"
#include <stdexcept>
#include "context/compiler_context.h"
#include "mlir/utils/operation/operation.h"
#define GET_MLIR_CONTEXT() compiler_context_->GetIRContext()->GetMLIRContext()
namespace middleend::mlir::lower {
// Get necessary information
MLIRLower::MLIRLower(context::CompilerContext *compiler_context) : compiler_context_(compiler_context) {
  module_op_ = GET_MLIR_CONTEXT()->GetModuleOp();
  // Reset the function map
  GET_MLIR_CONTEXT()->GetResourceContext()->ResetFuncMap();
  lower_helper_ = new LowerHelper(compiler_context_);
  new_module_op_ = GET_MLIR_CONTEXT()->CreateOperation<mlir::ModuleOp>();
}

// Lower the high-level MLIR to low-level MLIR
auto MLIRLower::Lower() -> int {
  lower_helper_->SetCurrentRegion(new_module_op_->GetRegion());
  lower_helper_->SetInsertPoint(new_module_op_->GetBlock());
  for (auto &op : module_op_->GetBlock()->operations_) {
    try {
      lower_helper_->LowerDispatch(op);
    } catch (std::logic_error) {
      delete lower_helper_;
      lower_helper_ = nullptr;
      delete new_module_op_;
      compiler_context_->Print("Logic Error: Failed to lower operation: " + op->Name() + "\n");
      return 1;
    } catch (std::runtime_error) {
      compiler_context_->Print("Runtime Error: Failed to lower operation: " + op->Name() + "\n");
      delete lower_helper_;
      lower_helper_ = nullptr;
      delete new_module_op_;
      return 2;
    } catch (...) {
      compiler_context_->Print("Unknown Error: Failed to lower operation: " + op->Name() + "\n");
      delete lower_helper_;
      lower_helper_ = nullptr;
      delete new_module_op_;
      return 3;
    }
  }
  // After lowering, we need to set the new module operation to the MLIR context
  GET_MLIR_CONTEXT()->SetModuleOp(new_module_op_);
  return 0;
}

}  // namespace middleend::mlir::lower