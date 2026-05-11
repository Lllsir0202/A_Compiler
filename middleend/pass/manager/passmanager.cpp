#include "middleend/pass/manager/passmanager.h"
#include "context/compiler_context.h"
namespace middleend::pass {
auto PassManager::AddPass(Pass *pass) -> void { passes_.emplace_back(pass); }

auto PassManager::RunPasses() -> void {
  for (auto pass : passes_) {
    if (!pass->Verify()) {
      // If verification failed, report error and stop running the pass
      compiler_context_->Print(pass->Name() + ": Pass verification failed, cannot run the pass\n");
      return;
    }
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