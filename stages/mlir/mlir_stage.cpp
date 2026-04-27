#include "stages/mlir/mlir_stage.h"

namespace stages {

auto MLIRStage::Run() -> void {
  auto module_op = compiler_context_->GetIRContext()->GetMLIRContext()->GetModuleOp();
  module_op->Emit(compiler_context_->GetOutputStream());
}

}  // namespace stages