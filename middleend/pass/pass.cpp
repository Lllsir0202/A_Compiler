#include "middleend/pass/pass.h"
#include "context/compiler_context.h"
namespace middleend::pass {
auto Pass::Verify() -> bool {
  for (auto &dep : dependencies_) {
    Pass *dep_pass = compiler_context_->GetPassContext()->GetPassByName(dep);
    // If the dependency pass is not found,
    // we can report an error and return false
    if (dep_pass == nullptr) {
      compiler_context_->Print(Name() + ": Dependency pass " + dep + " not found\n");
      return false;
    }
    // Check stage
    if (dep_pass->GetPassStage() != pass_stage_) {
      compiler_context_->Print(Name() + ": Dependency pass " + dep + " has different pass stage\n");
      return false;
    }
    // If dependency pass is found but not valid,
    // we can report an error and return false,
    // it is based on the assumption that we will run the dependency pass before verifying
    if (!dep_pass->IsValid()) {
      compiler_context_->Print(Name() + ": Dependency pass " + dep + " is invalid\n");
      return false;
    }
  }
  return true;
}

/**
 * @brief PreRun is a function that will be called before running the sepecific pass
 * it will make sure that all dependency passes which are invalidated rerun
 */
auto Pass::PreRun() -> void {
  for (auto &dep : dependencies_) {
    Pass *dep_pass = compiler_context_->GetPassContext()->GetPassByName(dep);
    if (dep_pass != nullptr && !dep_pass->IsValid()) {
      dep_pass->PreRun();
      if (!dep_pass->IsValid()) {
        dep_pass->Run();
      }
    }
  }
}

}  // namespace middleend::pass
