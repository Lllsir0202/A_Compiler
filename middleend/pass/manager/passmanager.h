#ifndef MIDDLEEND_PASS_MANAGER_PASSMANAGER_H
#define MIDDLEEND_PASS_MANAGER_PASSMANAGER_H
#include <unordered_map>
#include <vector>
#include "middleend/pass/pass.h"
namespace middleend::pass {
class PassManager {
 public:
  explicit PassManager(context::CompilerContext *compiler_context, PassStage current_pass_type)
      : compiler_context_(compiler_context), current_pass_type_(current_pass_type) {}
  ~PassManager() {
    for (auto pass : passes_) {
      delete pass;
    }
    passes_.clear();
  }
  auto AddPass(Pass *pass) -> void;
  auto VerifyPasses() -> bool;
  auto RunPasses() -> int;
  auto ResetPasses() -> void {
    passes_.clear();
    pass_name_to_pass_.clear();
    current_pass_type_ = PassStage::None;
  }

  // This function is used to get the pass by its name,
  // which will be used to deal with the pass dependencies in the future
  auto GetPassByName(const std::string &pass_name) -> Pass *;
  // Get the pass by its name and cast it to the specific pass type,
  // return nullptr if the pass is not found or the cast fails
  template <typename PassType>
  auto GetSpecPass(const std::string &pass_name) -> PassType * {
    Pass *pass = GetPassByName(pass_name);
    if (pass == nullptr) {
      return nullptr;
    }
    return dynamic_cast<PassType *>(pass);
  }

  auto SetCurrentPassType(PassStage pass_stage) -> void { current_pass_type_ = pass_stage; }
  auto GetCurrentPassType() -> PassStage { return current_pass_type_; }

  // Used for transform passes
  auto InvalidateAnalysis() -> void {
    for (auto pass : passes_) {
      if (pass->IsAnalysisPass()) {
        pass->Invalidate();
      }
    }
  }

 private:
  std::vector<Pass *> passes_;
  std::unordered_map<std::string, Pass *> pass_name_to_pass_;
  context::CompilerContext *compiler_context_;
  PassStage current_pass_type_{PassStage::None};
};

}  // namespace middleend::pass

#endif