#ifndef BACKEND_TARGETS_RISCV_ISEL_RISCVISEL_H
#define BACKEND_TARGETS_RISCV_ISEL_RISCVISEL_H

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "backend/targets/riscv/inst/riscvinst.h"
#include "backend/targets/utils/context/targetcontext.h"

namespace context {
class CompilerContext;
}
namespace backend::utils {
class Function;
class Block;
class Operand;
}  // namespace backend::utils
namespace backend::inst {
class Instruction;
}

namespace backend::targetir::riscv {

/**
 * @brief Instruction selection: MIR (target-independent, virtual regs) -> RISC-V
 * TargetInst (still virtual regs). Macro-expansion style with a compare/branch
 * fusion peephole. See docs/backend_codegen_plan.md §2. Bodies driven off MIR
 * use-lists / def-instructions, never off per-testcase shapes.
 *
 * Milestone M1: integer subset (arithmetic, shifts, logical, icmp, branches,
 * copy, call with <=8 integer args, return). Memory / float / stack-args land in
 * later milestones and are emitted as annotated TODO comments meanwhile.
 */
class RISCVISel {
 public:
  RISCVISel(context::CompilerContext *compiler_context, TargetContext *ctx)
      : compiler_context_(compiler_context), ctx_(ctx) {}

  /// Select a whole MIR function into a RISC-V TargetFunction (virtual regs).
  auto SelectFunction(backend::utils::Function *mir_func) -> TargetFunction *;

 private:
  // Per-instruction selection.
  auto SelectInst(backend::inst::Instruction *inst) -> void;
  auto SelectBinary(backend::inst::Instruction *inst, RVOpcode opcode) -> void;
  auto SelectICmp(backend::inst::Instruction *inst) -> void;
  auto SelectCondBr(backend::inst::Instruction *inst) -> void;
  auto SelectBr(backend::inst::Instruction *inst) -> void;
  auto SelectRet(backend::inst::Instruction *inst) -> void;
  auto SelectCopy(backend::inst::Instruction *inst) -> void;
  auto SelectCall(backend::inst::Instruction *inst) -> void;

  // Operand helpers.
  auto GetOrCreateVReg(backend::utils::Operand *mir_operand) -> TargetOperand *;
  auto GetReg(backend::utils::Operand *mir_operand) -> TargetOperand *;  // materializes literals via li
  auto Zero() -> TargetOperand *;                                        // x0
  auto PhysGPR(uint8_t id) -> TargetOperand *;
  auto LabelForBlock(size_t block_id) -> TargetLabel *;

  // Instruction emission helper: build a RISCVInst, wire result + operands, append.
  auto Emit(RVOpcode opcode, TargetOperand *result, const std::vector<TargetOperand *> &operands) -> RISCVInst *;
  auto Comment(const std::string &text) -> void;

  // True if an icmp result feeds exactly one user and that user is a CondBr,
  // so the compare can be folded into the branch instead of materialized.
  auto IsFusableToBranch(backend::inst::Instruction *icmp) const -> bool;

  context::CompilerContext *compiler_context_;
  TargetContext *ctx_;

  std::string func_name_;
  TargetBlock *cur_block_ = nullptr;
  std::unordered_map<backend::utils::Operand *, TargetOperand *> value_map_;
  std::unordered_map<size_t, TargetBlock *> block_by_id_;
  std::unordered_set<backend::inst::Instruction *> fused_icmps_;

  // Function-wide def-use summary used for compare/branch fusion: how many times
  // each register value is used, and (when used exactly once) its sole user.
  std::unordered_map<backend::utils::Operand *, int> use_count_;
  std::unordered_map<backend::utils::Operand *, backend::inst::Instruction *> one_user_;
};

}  // namespace backend::targetir::riscv

#endif
