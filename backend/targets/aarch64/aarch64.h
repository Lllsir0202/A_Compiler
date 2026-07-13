#ifndef BACKEND_TARGETS_AARCH64_AARCH64_H
#define BACKEND_TARGETS_AARCH64_AARCH64_H

#include <fstream>
#include <vector>
#include "backend/targets/utils/context/targetcontext.h"

namespace context {
class CompilerContext;
}

namespace backend::targetir::aarch64 {

/**
 * @brief Top-level AArch64 (ARMv8-A, AAPCS64/LP64) code-generation driver.
 * Mirrors RISCVBackend: consumes the target-independent MIR module and runs
 * ISel -> RegAlloc(graph coloring) -> Frame, then emits assembly.
 */
class AArch64Backend {
 public:
  explicit AArch64Backend(context::CompilerContext *compiler_context, int opt_level = 0, bool magic_div = true)
      : compiler_context_(compiler_context), opt_level_(opt_level), magic_div_(magic_div) {}

  auto Run() -> int;
  auto Emit(std::ofstream *out) -> void;

 private:
  auto EmitGlobals(std::ofstream *out) -> void;

  context::CompilerContext *compiler_context_;
  int opt_level_;
  bool magic_div_ = true;  // constant division -> magic multiply (vs sdiv)
  TargetContext ctx_;
  std::vector<TargetFunction *> target_functions_;
};

}  // namespace backend::targetir::aarch64

#endif
