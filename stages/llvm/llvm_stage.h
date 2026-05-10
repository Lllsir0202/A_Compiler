#ifndef STAGES_LLVM_LLVM_STAGE_H
#define STAGES_LLVM_LLVM_STAGE_H
#include "context/compiler_context.h"
#include "stages/stage.h"

namespace stages {
/**
 * @brief LLVMStage representing the LLVM middleend,
 * which is responsible for printing the LLVM IR
 */
class LLVMStage : public Stage {
 public:
  void Run() override;

  explicit LLVMStage(context::CompilerContext *compiler_context) : compiler_context_(compiler_context) {}
  ~LLVMStage() override { compiler_context_ = nullptr; }

 private:
  context::CompilerContext *compiler_context_;
};
}  // namespace stages

#endif