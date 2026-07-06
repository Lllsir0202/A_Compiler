#ifndef STAGES_ASSEMBLY_ASM_STAGE_H
#define STAGES_ASSEMBLY_ASM_STAGE_H

#include "context/compiler_context.h"
#include "stages/stage.h"
namespace stages {
class AssemblyStage : public Stage {
 public:
  void Run() override;

  explicit AssemblyStage(context::CompilerContext *compiler_context) : compiler_context_(compiler_context) {}
  ~AssemblyStage() override { compiler_context_ = nullptr; }

 private:
  context::CompilerContext *compiler_context_;
};

}  // namespace stages

#endif