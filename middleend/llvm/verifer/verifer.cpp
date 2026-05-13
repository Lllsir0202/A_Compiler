#include "middleend/llvm/verifer/verifer.h"
#include "context/compiler_context.h"
namespace middleend::llvm::verifer {
auto LLVMVerifer::Verify() -> bool {
  // Verify the module, check if all function definitions and instructions are valid
  if (module_ == nullptr) {
    compiler_context_->Print("Module: No module to verify.");
    return false;
  }
  for (auto function : module_->GetFunctions()) {
    if (function == nullptr) {
      compiler_context_->Print("Null function found in module");
      return false;
    }

    for (auto block : function->GetBasicBlocks()) {
      if (block == nullptr) {
        compiler_context_->Print("Null block found in function: " + function->GetFuncName());
        return false;
      }
      for (auto inst : block->GetInstructions()) {
        if (inst == nullptr) {
          compiler_context_->Print("Null instruction found in function: " + function->GetFuncName());
          return false;
        }
        if (!inst->Verify(compiler_context_)) {
          compiler_context_->Print("Instruction verification failed in function: " + function->GetFuncName());
          return false;
        }
      }
    }
  }
  return true;
}

}  // namespace middleend::llvm::verifer