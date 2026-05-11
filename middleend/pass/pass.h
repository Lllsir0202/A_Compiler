#ifndef MIDDLEEND_PASS_PASS_H
#define MIDDLEEND_PASS_PASS_H
#include <cstdint>
#include <string>
namespace context {
class CompilerContext;
}
namespace middleend::pass {
// A pass must specify which stage it belongs to,
// so that we can verify if the pass can be run on the current stage
enum class PassStage : uint8_t { MLIR, BASEIR, LLVMIR };

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
  virtual auto Verify() -> bool { return true; }
  virtual auto Name() const -> std::string = 0;
  auto GetPassStage() -> PassStage { return pass_stage_; }

 protected:
  explicit Pass(context::CompilerContext *compiler_context, PassStage pass_stage)
      : compiler_context_(compiler_context), pass_stage_(pass_stage) {}
  context::CompilerContext *compiler_context_;
  PassStage pass_stage_;
};

}  // namespace middleend::pass

#endif