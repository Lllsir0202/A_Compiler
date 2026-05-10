#ifndef STAGES_MLIR_MLIR_STAGE_H
#define STAGES_MLIR_MLIR_STAGE_H
#include "context/compiler_context.h"
#include "stages/stage.h"

namespace stages {
/**
 * @brief MLIRStage representing the MLIR middleend,
 * since the dialect could be in different level,
 * the dialect we are using in a block could be different
 */
class MLIRStage : public Stage {
 public:
  void Run() override;

  explicit MLIRStage(context::CompilerContext *compiler_context) : compiler_context_(compiler_context) {}
  ~MLIRStage() override { compiler_context_ = nullptr; }

 private:
  context::CompilerContext *compiler_context_;
};
}  // namespace stages

#endif