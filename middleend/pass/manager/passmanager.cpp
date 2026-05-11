#include "middleend/pass/manager/passmanager.h"
#include "context/compiler_context.h"
#include "middleend/pass/pass.h"
namespace middleend::pass {
auto PassManager::AddPass(Pass *pass) -> void { passes_.emplace_back(pass); }

// Verify all passes, make sure all passes are same as the current pass type,
// and all passes' dependencies are satisfied, etc.
auto PassManager::VerifyPasses() -> bool {
  for (auto pass : passes_) {
    if (pass->GetPassStage() != current_pass_type_) {
      compiler_context_->Print(pass->Name() + ": Pass stage mismatch, expected " +
                               Pass::PassStageToString(current_pass_type_) + ", but got " +
                               Pass::PassStageToString(pass->GetPassStage()) + "\n");
      return false;
    }
    if (!pass->Verify()) {
      compiler_context_->Print(pass->Name() + ": Pass verification failed\n");
      return false;
    }
  }
  return true;
}

auto PassManager::RunPasses() -> void {
  VerifyPasses();
  for (auto pass : passes_) {
    try {
      pass->Run();
    } catch (const std::exception &e) {
      compiler_context_->Print(pass->Name() + ": Error occurred while running the pass: " + e.what() + "\n");
      // Free all resources allocated
      return;
    }
  }
}

auto PassManager::GetPassByName(const std::string &pass_name) -> Pass * {
  if (pass_name_to_pass_.count(pass_name) > 0) {
    return pass_name_to_pass_[pass_name];
  }
  // Not found, return nullptr,
  // might need to create and run the pass in the future
  return nullptr;
}
}  // namespace middleend::pass