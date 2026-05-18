#ifndef CONTEXT_PASS_CONTEXT_H
#define CONTEXT_PASS_CONTEXT_H
#include <unordered_set>
#include "middleend/mlir/utils/value/value.h"
#include "middleend/pass/manager/passmanager.h"
#include "middleend/pass/pass.h"
namespace context {
class CompilerContext;
class PassContext {
 public:
  explicit PassContext(CompilerContext *compiler_context, middleend::pass::PassStage pass_stage)
      : compiler_context_(compiler_context) {
    pass_manager_ = new middleend::pass::PassManager(compiler_context, pass_stage);
  }

  auto AddPass(middleend::pass::Pass *pass) -> void { pass_manager_->AddPass(pass); }
  auto RunPasses() -> int { return pass_manager_->RunPasses(); }

  // Interfaces to get a pass by name
  auto GetPassByName(const std::string &pass_name) -> middleend::pass::Pass * {
    return pass_manager_->GetPassByName(pass_name);
  }

  template <typename PassType, typename... Args>
  static auto CreatePass(Args &&...args) -> PassType * {
    return new PassType(std::forward<Args>(args)...);
  }

  template <typename PassType>
  auto GetSpecPass(const std::string &pass_name) -> PassType * {
    return pass_manager_->GetSpecPass<PassType>(pass_name);
  }

  // Invalidate all the analysis passes
  auto InvalidateAnalysis() -> void { pass_manager_->InvalidateAnalysis(); }

  auto InsertDeletedValue(middleend::mlir::Value *value) -> void {
    if (deleted_values_.count(value) > 0) {
      return;
    }
    deleted_values_.insert(value);
  }

  auto IsDeletedValue(middleend::mlir::Value *value) -> bool { return deleted_values_.count(value) > 0; }

 private:
  [[maybe_unused]] CompilerContext *compiler_context_;
  middleend::pass::PassManager *pass_manager_;

  std::unordered_set<middleend::mlir::Value *> deleted_values_;
};

}  // namespace context

#endif