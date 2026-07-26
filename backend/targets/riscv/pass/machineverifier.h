#ifndef BACKEND_TARGETS_RISCV_PASS_MACHINEVERIFIER_H
#define BACKEND_TARGETS_RISCV_PASS_MACHINEVERIFIER_H

#include <string>

namespace context {
class CompilerContext;
}

namespace backend::targetir {
class TargetFunction;
class TargetInst;
}  // namespace backend::targetir

namespace backend::targetir::riscv {

enum class MachineVerifyStage {
  PreRegisterAllocation,
  PostPhiElimination,
  PostRegisterAllocation,
};

/**
 * Structural verifier for selected RISC-V machine IR.
 *
 * The verifier deliberately runs both before and after register allocation.
 * The first check catches malformed instruction selection / machine-pass
 * rewrites while virtual registers still carry useful identity. The second
 * additionally rejects every remaining virtual register and validates the
 * instructions introduced by spill and frame lowering.
 */
class RISCVVerifier {
 public:
  explicit RISCVVerifier(context::CompilerContext *compiler_context) : compiler_context_(compiler_context) {}

  auto Verify(TargetFunction *func, MachineVerifyStage stage) -> bool;

 private:
  auto VerifyInst(TargetInst *inst, MachineVerifyStage stage, const std::string &location) -> bool;
  auto Error(const std::string &message) -> bool;

  context::CompilerContext *compiler_context_;
};

}  // namespace backend::targetir::riscv

#endif
