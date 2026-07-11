#ifndef BACKEND_TARGETS_RISCV_RISCV_H
#define BACKEND_TARGETS_RISCV_RISCV_H

#include <fstream>
#include <vector>

namespace context {
class CompilerContext;
}
namespace backend::targetir {
class TargetFunction;
}

namespace backend::targetir::riscv {

/**
 * @brief Top-level RISC-V (rv64, lp64d) code-generation driver.
 *
 * Consumes the target-independent MIR module from the CompilerContext and runs
 * the full backend pipeline, then emits assembly. See docs/backend_codegen_plan.md.
 *
 *   Run():   MIR -> ISel -> (peephole) -> RegAlloc(graph coloring) -> Frame/PEI
 *            -> post-RA peephole, producing target_functions_.
 *   Emit():  RISCVPrinter renders target_functions_ + globals as a .s file.
 *
 * Milestone M0 (current): the pipeline is wired but its phases are stubs; Emit
 * writes a placeholder assembly header so the -S stage is verifiably end-to-end.
 */
class RISCVBackend {
 public:
  explicit RISCVBackend(context::CompilerContext *compiler_context) : compiler_context_(compiler_context) {}

  /// Run the whole codegen pipeline over the MIR module. Returns 0 on success.
  auto Run() -> int;

  /// Emit the generated assembly to the given stream.
  auto Emit(std::ofstream *out) -> void;

 private:
  context::CompilerContext *compiler_context_;

  // Selected + allocated functions, ready for emission (populated by Run()).
  std::vector<TargetFunction *> target_functions_;
};

}  // namespace backend::targetir::riscv

#endif
