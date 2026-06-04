#include "stages/mir/mir_stage.h"

namespace stages {
auto MIRStage::Run() -> void {
  auto module = compiler_context_->GetMIRContext()->GetModule();
  module->Emit(compiler_context_->GetOutputStream());
}

}  // namespace stages