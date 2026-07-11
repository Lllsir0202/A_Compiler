#ifndef STAGES_ASM_ASM_STAGE_H
#define STAGES_ASM_ASM_STAGE_H

#include "backend/targets/riscv/riscv.h"
#include "context/compiler_context.h"
#include "stages/stage.h"

namespace stages {

// Emits the target assembly (.s) produced by the backend to the output stream.
// Mirrors MIRStage: the stage only does output; the backend does the work.
class AsmStage : public Stage {
 public:
  AsmStage(context::CompilerContext *compiler_context, backend::targetir::riscv::RISCVBackend *backend)
      : compiler_context_(compiler_context), backend_(backend) {}
  ~AsmStage() override { compiler_context_ = nullptr; }

  void Run() override;

 private:
  context::CompilerContext *compiler_context_;
  backend::targetir::riscv::RISCVBackend *backend_;
};

}  // namespace stages

#endif
