#include "middleend/mlir/lower/lower.h"
#include "context/compiler_context.h"

namespace middleend::mlir::lower {
// Get necessary information
MLIRLower::MLIRLower(context::CompilerContext *compiler_context) : compiler_context_(compiler_context) {
  module_op_ = compiler_context_->GetIRContext()->GetMLIRContext()->GetModuleOp();
}

// Lower the high-level MLIR to low-level MLIR
auto MLIRLower::Lower() -> void {}

}  // namespace middleend::mlir::lower