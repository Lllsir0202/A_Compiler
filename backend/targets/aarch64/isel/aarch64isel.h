#ifndef BACKEND_TARGETS_AARCH64_ISEL_AARCH64ISEL_H
#define BACKEND_TARGETS_AARCH64_ISEL_AARCH64ISEL_H

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "backend/targets/aarch64/inst/aarch64inst.h"
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

namespace backend::targetir::aarch64 {

/**
 * @brief Instruction selection: MIR (target-independent, virtual regs) -> AArch64
 * AArch64Inst (still virtual regs). Macro-expansion style with a compare/branch
 * fusion peephole, mirroring RISCVISel. Bodies are driven off MIR use-lists /
 * def-instructions, never off per-testcase shapes.
 */
class AArch64ISel {
 public:
  AArch64ISel(context::CompilerContext *compiler_context, TargetContext *ctx, bool magic_div = true)
      : compiler_context_(compiler_context), ctx_(ctx), magic_div_(magic_div) {}

  auto SelectFunction(backend::utils::Function *mir_func) -> TargetFunction *;
  auto AllocaBytes() const -> size_t { return alloca_bytes_; }
  auto OutgoingBytes() const -> size_t { return outgoing_bytes_; }

 private:
  auto SelectInst(backend::inst::Instruction *inst) -> void;
  auto SelectBinary(backend::inst::Instruction *inst, A64Opcode opcode) -> void;
  auto SelectDiv(backend::inst::Instruction *inst) -> void;
  auto SelectMod(backend::inst::Instruction *inst) -> void;
  // Emit a signed division of `numer` by the constant `divisor` (no `sdiv`),
  // returning the quotient register, or nullptr if the divisor is not handled
  // (|d| < 2, d == INT_MIN) and the caller should fall back to `sdiv`.
  auto EmitConstDivide(TargetOperand *numer, int divisor) -> TargetOperand *;
  auto SelectICmp(backend::inst::Instruction *inst) -> void;
  auto SelectFCmp(backend::inst::Instruction *inst) -> void;
  auto SelectConvert(backend::inst::Instruction *inst, A64Opcode opcode) -> void;
  auto SelectCondBr(backend::inst::Instruction *inst) -> void;
  auto SelectBr(backend::inst::Instruction *inst) -> void;
  auto SelectRet(backend::inst::Instruction *inst) -> void;
  auto SelectCopy(backend::inst::Instruction *inst) -> void;
  auto SelectCall(backend::inst::Instruction *inst) -> void;
  auto SelectLoad(backend::inst::Instruction *inst) -> void;
  auto SelectStore(backend::inst::Instruction *inst) -> void;

  // Materialize a MIR pointer operand (frame slot, global, GEP, or a register
  // already holding an address) as an x-register holding the address.
  auto LowerAddress(backend::utils::Operand *ptr) -> TargetOperand *;
  auto AllocaOffset(backend::utils::Operand *frame_op) -> size_t;

  auto GetOrCreateVReg(backend::utils::Operand *mir_operand) -> TargetOperand *;
  auto GetReg(backend::utils::Operand *mir_operand) -> TargetOperand *;  // materializes literals
  auto PhysGPR(uint8_t id, TargetRegSize size) -> TargetOperand *;
  auto PhysFPR(uint8_t id) -> TargetOperand *;
  auto LabelForBlock(size_t block_id) -> TargetLabel *;

  auto Emit(A64Opcode opcode, TargetOperand *result, const std::vector<TargetOperand *> &operands) -> AArch64Inst *;
  auto EmitCond(A64Opcode opcode, TargetOperand *result, A64Cond cond,
                const std::vector<TargetOperand *> &operands) -> AArch64Inst *;
  auto Comment(const std::string &text) -> void;

  // Materialize a 32-bit constant into `dst` via movz / movn / movk.
  auto EmitMovImm(TargetOperand *dst, int value) -> void;
  // Emit a legalized load/store relative to a base register (sp/fp), computing an
  // out-of-range offset in the x16 scratch first.
  auto EmitBaseLoad(A64Opcode opcode, TargetOperand *dst, uint8_t base_id, int offset) -> void;
  auto EmitBaseStore(A64Opcode opcode, TargetOperand *value, uint8_t base_id, int offset) -> void;

  auto IsFusableToBranch(backend::inst::Instruction *cmp) const -> bool;

  context::CompilerContext *compiler_context_;
  TargetContext *ctx_;
  bool magic_div_ = true;  // constant division -> magic multiply (vs sdiv)

  std::string func_name_;
  TargetBlock *cur_block_ = nullptr;
  std::unordered_map<backend::utils::Operand *, TargetOperand *> value_map_;
  std::unordered_map<size_t, TargetBlock *> block_by_id_;
  std::unordered_set<backend::inst::Instruction *> fused_cmps_;
  std::unordered_map<backend::utils::Operand *, int> use_count_;
  std::unordered_map<backend::utils::Operand *, backend::inst::Instruction *> one_user_;

  std::unordered_map<backend::utils::Operand *, size_t> alloca_offset_;
  size_t alloca_bytes_ = 0;
  size_t outgoing_bytes_ = 0;

  // Global-address reuse: a global's address (adrp + add :lo12:) is loop- and
  // function-invariant, so materialize it once per function in the entry block
  // (which dominates every use) and share it, instead of recomputing at each
  // access. Filled by LowerAddress, flushed into the entry block by SelectFunction.
  TargetBlock *entry_block_ = nullptr;
  std::unordered_map<std::string, TargetOperand *> global_addr_;
  std::vector<TargetInst *> entry_globals_;
};

}  // namespace backend::targetir::aarch64

#endif
