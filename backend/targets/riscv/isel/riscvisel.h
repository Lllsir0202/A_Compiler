#ifndef BACKEND_TARGETS_RISCV_ISEL_RISCVISEL_H
#define BACKEND_TARGETS_RISCV_ISEL_RISCVISEL_H

namespace context {
class CompilerContext;
}
namespace backend::utils {
class Function;
class Module;
}  // namespace backend::utils
namespace backend::targetir {
class TargetFunction;
}

namespace backend::targetir::riscv {

/**
 * @brief Instruction selection: MIR (target-independent, virtual regs) -> RISC-V
 * TargetInst (still virtual regs, ABI/fixed constraints expressed as pre-colored
 * copies). Macro-expansion style with peephole pattern matches (see
 * docs/backend_codegen_plan.md §2).
 *
 * Selection rules of note, all driven off MIR use-lists (no per-testcase special
 * casing):
 *   - int ops use the .w variants (addw/subw/mulw/divw/remw/sllw/...) to keep
 *     32-bit wrap-around semantics; address arithmetic stays 64-bit.
 *   - an ICmp whose only use is an adjacent CondBr fuses into beq/bne/blt/bge/...
 *   - phi nodes are lowered to parallel copies before/within selection.
 *   - every immediate goes through one legalization helper (|imm| <= 2047 else
 *     li + reg), so oversized immediates are handled in exactly one place.
 *
 * Skeleton: declares the contract; bodies land in milestone M1/M2.
 */
class RISCVISel {
 public:
  explicit RISCVISel(context::CompilerContext *compiler_context) : compiler_context_(compiler_context) {}

  /// Select a whole MIR function into a RISC-V TargetFunction (virtual regs).
  auto SelectFunction(backend::utils::Function *mir_func) -> TargetFunction *;

 private:
  context::CompilerContext *compiler_context_;
};

}  // namespace backend::targetir::riscv

#endif
