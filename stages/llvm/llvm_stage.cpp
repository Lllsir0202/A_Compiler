#include "stages/llvm/llvm_stage.h"

namespace stages {
auto LLVMStage::Run() -> void {
  auto module = compiler_context_->GetIRContext()->GetLLVMIRContext()->GetModule();
  module->Emit(compiler_context_->GetOutputStream());
}
}  // namespace stages