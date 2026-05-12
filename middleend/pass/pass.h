#ifndef MIDDLEEND_PASS_PASS_H
#define MIDDLEEND_PASS_PASS_H
#include <cstdint>
#include <string>
#include <vector>
namespace context {
class CompilerContext;
}
namespace middleend::pass {
// A pass must specify which stage it belongs to,
// so that we can verify if the pass can be run on the current stage
enum class PassStage : uint8_t { MLIR, BASEIR, LLVMIR, None };

// Base class for all passes
class Pass {
 public:
  static auto PassStageToString(PassStage pass_stage) -> std::string {
    switch (pass_stage) {
      case PassStage::MLIR:
        return "MLIR";
      case PassStage::BASEIR:
        return "BASEIR";
      case PassStage::LLVMIR:
        return "LLVMIR";
      default:
        return "Unknown";
    }
  }
  virtual ~Pass() = default;
  virtual void Run() = 0;
  // Verify the pass, check if the pass can be run on the current stage,
  // also check if the pass's dependencies are satisfied, etc.
  virtual auto Verify() -> bool;
  virtual auto Name() const -> std::string = 0;
  virtual auto PreRun() -> void;
  auto GetPassStage() -> PassStage { return pass_stage_; }
  virtual auto IsValid() -> bool { return valid_; }
  // Every time run the pass, we will call the validate
  // For transform, it is useless
  virtual auto Validate() -> void { valid_ = true; }
  virtual auto Invalidate() -> void { valid_ = false; }
  virtual auto IsAnalysisPass() -> bool = 0;

  // Interfaces to deal with the pass dependencies
  [[maybe_unused]] auto AddDependency(const std::string &pass_name) -> void { dependencies_.emplace_back(pass_name); }
  auto SetDependencies(const std::vector<std::string> &dependencies) -> void { dependencies_ = dependencies; }
  auto GetDependencies() -> std::vector<std::string> { return dependencies_; }

 protected:
  explicit Pass(context::CompilerContext *compiler_context, PassStage pass_stage)
      : compiler_context_(compiler_context), pass_stage_(pass_stage) {}
  context::CompilerContext *compiler_context_;
  PassStage pass_stage_;
  // If the pass has dependencies, we can add a list of pass names here,
  // and check if the dependencies are satisfied in the Verify function in the future
  std::vector<std::string> dependencies_;
  // We can add the valid or not of the pass,
  // since some passes might be invalidated after some other passes are run,
  // especially for the transformation passes,
  // which will change the IR structure a lot,
  bool valid_{false};
};

}  // namespace middleend::pass

#endif