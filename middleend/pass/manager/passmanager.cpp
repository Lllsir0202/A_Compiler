#include "middleend/pass/manager/passmanager.h"
#include <functional>
#include <unordered_map>
#include "context/compiler_context.h"
#include "middleend/pass/pass.h"
namespace middleend::pass {
auto PassManager::AddPass(Pass *pass) -> void {
  // Map the pass name to the pass instance for future use
  // Add check to make sure the pass stage is same as the current pass type
  if (pass->GetPassStage() != current_pass_type_) {
    compiler_context_->Print(pass->Name() + ": Pass stage mismatch, expected " +
                             Pass::PassStageToString(current_pass_type_) + ", but got " +
                             Pass::PassStageToString(pass->GetPassStage()) + "\n");
    return;
  }
  // Do not allow duplicate pass names.
  if (pass_name_to_pass_.count(pass->Name()) > 0) {
    compiler_context_->Print("Pass with name " + pass->Name() + " already exists\n");
    delete pass;
    return;
  }
  pass_name_to_pass_[pass->Name()] = pass;
  passes_.emplace_back(pass);
}

// Verify all passes, make sure all passes are same as the current pass type,
// and all passes' dependencies are satisfied, etc.
auto PassManager::VerifyPasses() -> bool {
  // Must set the current pass type before adding passes,
  // otherwise we cannot verify the passes
  if (current_pass_type_ == PassStage::None) {
    compiler_context_->Print("Current pass type is None, cannot verify passes\n");
    return false;
  }
  for (auto pass : passes_) {
    if (pass->GetPassStage() != current_pass_type_) {
      compiler_context_->Print(pass->Name() + ": Pass stage mismatch, expected " +
                               Pass::PassStageToString(current_pass_type_) + ", but got " +
                               Pass::PassStageToString(pass->GetPassStage()) + "\n");
      return false;
    }
  }
  return true;
}

auto PassManager::RunPasses() -> int {
  if (!VerifyPasses()) {
    compiler_context_->Print("Pass verification failed, cannot run passes\n");
    return 1;
  }

  std::vector<Pass *> ordered_passes;
  std::unordered_map<Pass *, int> visit_state;
  std::function<bool(Pass *)> dfs = [&](Pass *pass) -> bool {
    if (visit_state[pass] == 2) {
      return true;
    }
    if (visit_state[pass] == 1) {
      compiler_context_->Print("Cyclic pass dependency detected at " + pass->Name() + "\n");
      return false;
    }
    visit_state[pass] = 1;
    for (const auto &dependency_name : pass->GetDependencies()) {
      auto *dependency_pass = GetPassByName(dependency_name);
      if (dependency_pass == nullptr) {
        compiler_context_->Print(pass->Name() + ": Dependency pass " + dependency_name + " not found\n");
        return false;
      }
      if (!dfs(dependency_pass)) {
        return false;
      }
    }
    visit_state[pass] = 2;
    ordered_passes.emplace_back(pass);
    return true;
  };

  for (auto *pass : passes_) {
    if (!dfs(pass)) {
      compiler_context_->Print("Pass dependency resolution failed\n");
      return 1;
    }
  }

  for (auto *pass : ordered_passes) {
    try {
      pass->Run();
    } catch (const std::exception &e) {
      compiler_context_->Print(pass->Name() + ": Error occurred while running the pass: " + e.what() + "\n");
      // Free all resources allocated
      return 1;
    }
  }
  return 0;
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
