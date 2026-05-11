#ifndef MIDDLEEND_PASS_MANAGER_PASSMANAGER_H
#define MIDDLEEND_PASS_MANAGER_PASSMANAGER_H
#include <unordered_map>
#include <vector>
#include "middleend/pass/pass.h"
namespace middleend::pass {
class PassManager {
 public:
  explicit PassManager(context::CompilerContext *compiler_context) : compiler_context_(compiler_context) {}
  ~PassManager() {
    for (auto pass : passes_) {
      delete pass;
    }
    passes_.clear();
  }
  auto AddPass(Pass *pass) -> void;
  auto RunPasses() -> void;

  // This function is used to get the pass by its name,
  // which will be used to deal with the pass dependencies in the future
  auto GetPassByName(const std::string &pass_name) -> Pass *;

 private:
  std::vector<Pass *> passes_;
  std::unordered_map<std::string, Pass *> pass_name_to_pass_;
  context::CompilerContext *compiler_context_;
};

}  // namespace middleend::pass

#endif