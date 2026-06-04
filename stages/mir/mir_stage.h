#ifndef STAGES_MIR_MIR_STAGE_H
#define STAGES_MIR_MIR_STAGE_H

#include "context/compiler_context.h"
#include "stages/stage.h"
namespace stages {
class MIRStage : public Stage {
 public:
  void Run() override;

  explicit MIRStage(context::CompilerContext *compiler_context) : compiler_context_(compiler_context) {}
  ~MIRStage() override { compiler_context_ = nullptr; }

 private:
  context::CompilerContext *compiler_context_;
};

}  // namespace stages

#endif