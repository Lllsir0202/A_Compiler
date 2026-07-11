#include "stages/asm/asm_stage.h"

namespace stages {
auto AsmStage::Run() -> void { backend_->Emit(compiler_context_->GetOutputStream()); }

}  // namespace stages
