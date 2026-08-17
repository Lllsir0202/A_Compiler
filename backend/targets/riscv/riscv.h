#ifndef BACKEND_TARGETS_RISCV_RISCV_H
#define BACKEND_TARGETS_RISCV_RISCV_H

#include <fstream>
#include <vector>
#include "backend/targets/utils/context/targetcontext.h"

namespace context {
class CompilerContext;
}

namespace backend::targetir::riscv {

/**
 * @brief Top-level RISC-V (rv64, lp64d) code-generation driver.
 *
 * Consumes the target-independent MIR module from the CompilerContext and runs
 * the backend pipeline, then emits assembly. See docs/backend_codegen_plan.md.
 *
 *   Run():   MIR -> ISel -> (peephole) -> RegAlloc(graph coloring) -> Frame/PEI,
 *            producing target_functions_.
 *   Emit():  renders target_functions_ + globals as a .s file.
 *
 * Milestone M1: instruction selection is implemented for the integer subset;
 * register allocation and frame lowering are the next milestones, so emitted
 * code still contains virtual registers (v0, v1, ...) and a bare `ret`.
 */
class RISCVBackend {
 public:
  explicit RISCVBackend(context::CompilerContext *compiler_context, int opt_level = 0, bool magic_div = true,
                        bool rvv_enabled = false)
      : compiler_context_(compiler_context),
        opt_level_(opt_level),
        magic_div_(magic_div),
        rvv_enabled_(rvv_enabled) {}

  /// Run the whole codegen pipeline over the MIR module. Returns 0 on success.
  auto Run() -> int;

  /// Emit the generated assembly to the given stream.
  auto Emit(std::ofstream *out) -> void;

 private:
  auto EmitGlobals(std::ofstream *out) -> void;

  context::CompilerContext *compiler_context_;

  // Optimization level (0 = none). At >=1 the machine-level pass pipeline runs.
  int opt_level_;

  // Turn constant division/modulo into a magic multiply (vs divw/remw). On.
  bool magic_div_ = true;

  // Lower vector intrinsics to RVV (only meaningful for the riscv target, and only
  // when the middle-end vectorizer was enabled via -rvv). Off by default.
  bool rvv_enabled_ = false;

  // Owns all target IR (operands, instructions, blocks, functions).
  TargetContext ctx_;

  // Selected functions, ready for emission (populated by Run()).
  std::vector<TargetFunction *> target_functions_;
};

}  // namespace backend::targetir::riscv

#endif
