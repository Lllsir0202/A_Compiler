#include "stages/assembly/asm_stage.h"

namespace stages {
auto AssemblyStage::Run() -> void {
  auto module = compiler_context_->GetBackendContext()->GetModule();
  module->Emit(compiler_context_->GetOutputStream());
}

}  // namespace stages