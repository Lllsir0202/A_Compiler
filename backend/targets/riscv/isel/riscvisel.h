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

  /// Total stack bytes reserved for this function's allocas (arrays / address-
  /// taken locals); consumed by the register allocator and frame layer.
  auto AllocaBytes() const -> size_t { return alloca_bytes_; }

  /// Bytes reserved at the bottom of the frame for outgoing stack arguments
  /// (the 9th and later arguments of the widest call this function makes).
  auto OutgoingBytes() const -> size_t { return outgoing_bytes_; }

 private:
  // Per-instruction selection.
  auto SelectInst(backend::inst::Instruction *inst) -> void;
  auto SelectBinary(backend::inst::Instruction *inst, RVOpcode opcode) -> void;
  auto SelectICmp(backend::inst::Instruction *inst) -> void;
  auto SelectFCmp(backend::inst::Instruction *inst) -> void;
  auto SelectConvert(backend::inst::Instruction *inst, RVOpcode opcode) -> void;
  auto SelectCondBr(backend::inst::Instruction *inst) -> void;
  auto SelectBr(backend::inst::Instruction *inst) -> void;
  auto SelectRet(backend::inst::Instruction *inst) -> void;
  auto SelectCopy(backend::inst::Instruction *inst) -> void;
  auto SelectCall(backend::inst::Instruction *inst) -> void;
  auto SelectLoad(backend::inst::Instruction *inst) -> void;
  auto SelectStore(backend::inst::Instruction *inst) -> void;

  // Materialize the address denoted by a MIR pointer operand (frame slot, global
  // symbol, GEP memory operand, or a register already holding an address) into a
  // fresh virtual register.
  auto LowerAddress(backend::utils::Operand *ptr) -> TargetOperand *;
  // Byte offset of an alloca frame slot within this function's alloca area.
  auto AllocaOffset(backend::utils::Operand *frame_op) -> size_t;

  // Operand helpers.
  auto GetOrCreateVReg(backend::utils::Operand *mir_operand) -> TargetOperand *;
  auto GetReg(backend::utils::Operand *mir_operand) -> TargetOperand *;  // materializes literals via li
  auto Zero() -> TargetOperand *;                                        // x0
  auto PhysGPR(uint8_t id) -> TargetOperand *;
  auto PhysFPR(uint8_t id) -> TargetOperand *;
  auto LabelForBlock(size_t block_id) -> TargetLabel *;

  // Instruction emission helper: build a RISCVInst, wire result + operands, append.
  auto Emit(RVOpcode opcode, TargetOperand *result, const std::vector<TargetOperand *> &operands) -> RISCVInst *;
  auto Comment(const std::string &text) -> void;

  // Emit a load/store relative to a physical base register (sp/fp), legalizing an
  // offset outside the ±2047 immediate range by computing the address in a fresh
  // scratch vreg first. Used for incoming/outgoing stack arguments.
  auto EmitBaseLoad(RVOpcode opcode, TargetOperand *dst, uint8_t base_id, int offset) -> void;
  auto EmitBaseStore(RVOpcode opcode, TargetOperand *value, uint8_t base_id, int offset) -> void;

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

  // Alloca (stack storage) layout for this function.
  std::unordered_map<backend::utils::Operand *, size_t> alloca_offset_;
  size_t alloca_bytes_ = 0;
  // Outgoing stack-argument area, sized from the widest call (pre-scanned).
  size_t outgoing_bytes_ = 0;
};

}  // namespace backend::targetir::riscv

#endif
