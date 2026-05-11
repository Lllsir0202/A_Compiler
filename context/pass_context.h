#ifndef CONTEXT_PASS_CONTEXT_H
#define CONTEXT_PASS_CONTEXT_H
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
  auto RunPasses() -> void { pass_manager_->RunPasses(); }

  template <typename PassType, typename... Args>
  static auto CreatePass(Args &&...args) -> PassType * {
    return new PassType(std::forward<Args>(args)...);
  }

 private:
  [[maybe_unused]] CompilerContext *compiler_context_;
  middleend::pass::PassManager *pass_manager_;
};

}  // namespace context

#endif