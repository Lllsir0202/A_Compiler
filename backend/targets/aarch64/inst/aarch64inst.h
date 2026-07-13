#ifndef BACKEND_TARGETS_AARCH64_INST_AARCH64INST_H
#define BACKEND_TARGETS_AARCH64_INST_AARCH64INST_H

#include <cstdint>
#include <fstream>
#include <string>
#include <utility>
#include "backend/targets/utils/inst/targetinst.h"

namespace backend::targetir::aarch64 {

/**
 * @brief AArch64 (A64) opcodes used by the backend. Mnemonics are width-agnostic
 * — the 32 vs 64-bit width is carried by the register operands (w/x, s/d), not by
 * the mnemonic (contrast RISC-V's addw vs add). SysY int is 32-bit (w-view), a
 * pointer is 64-bit (x-view), a float is 32-bit single (s-view).
 */
enum class A64Opcode : uint8_t {
  // R-type (rd, rn, rm)
  ADD, SUB, MUL, SDIV,
  AND, ORR, EOR,
  LSLV, LSRV, ASRV,          // register-amount shifts
  // Multiply-accumulate (rd, rn, rm, ra)
  MSUB, MADD,                // msub: rd = ra - rn*rm  (used for modulo)
  // Immediate (rd, rn, #imm)
  ADD_I, SUB_I,
  AND_I, ORR_I, EOR_I,
  LSL_I, LSR_I, ASR_I,       // shift by immediate
  // Moves / constant materialization
  MOV,                       // mov rd, rn   (register move)
  MOVZ, MOVN,                // movz/movn rd, #imm16
  MOVK,                      // movk rd, #imm16, lsl #shift
  SXTW,                      // sxtw xd, wn  (sign-extend 32->64 for address math)
  // Compare + conditional set / branch (condition in cond_)
  CMP, CMP_I,                // cmp rn, rm | cmp rn, #imm  (sets flags)
  CSET,                      // cset rd, <cond>
  B_COND,                    // b.<cond> label
  B,                         // b label
  CBZ, CBNZ,                 // cbz/cbnz rn, label
  // Memory  (int=w, float=s, ptr=x, chosen by operand size/class)
  LDR_OFF, STR_OFF,          // ldr rt, [rn, #imm] | str rt, [rn, #imm]
  LDR_REG, STR_REG,          // ldr rt, [rn, rm, lsl #shift]
  // Address materialization
  ADRP,                      // adrp xd, sym
  ADD_LO12,                  // add xd, xn, #:lo12:sym
  // Single-precision float
  FADD, FSUB, FMUL, FDIV,    // R-type, s operands
  FCMP,                      // fcmp sn, sm  (sets flags)
  FMOV,                      // fmov sd, sn  (float register move)
  FMOV_TO_GPR, FMOV_FROM_GPR,// fmov wd, sn | fmov sd, wn  (bit move)
  SCVTF,                     // scvtf sd, wn  (int -> float)
  FCVTZS,                    // fcvtzs wd, sn (float -> int, truncate)
  // Calls / control
  BL,                        // bl sym
  RET,                       // ret
  COMMENT,                   // "// text" pseudo
  SMULH,                     // pseudo: rd = high 32 of signed (rn * rm); expands to
                             // `smull x16, wn, wm; asr x16,x16,#32; mov wd,w16`
};

/// AArch64 condition codes used by cset / b.<cond>. SysY comparisons are signed
/// and NaN-free, so the ordered signed codes are exact for both int (after cmp)
/// and float (after fcmp).
enum class A64Cond : uint8_t { None, EQ, NE, LT, LE, GT, GE };

auto A64CondName(A64Cond cond) -> const char *;

/// How an opcode renders its operands.
enum class A64Form : uint8_t {
  R3, R4, RI, MovReg, MovImm, MovK, Unary,
  Cmp, CmpImm, Cset, BCond, Branch, CbzForm,
  LoadOff, StoreOff, LoadReg, StoreReg, Adrp, AddLo12,
  FCmpForm, Call, Ret,
};

struct A64OpcodeInfo {
  const char *mnemonic_;
  A64Form form_;
};

auto GetA64OpcodeInfo(A64Opcode opcode) -> const A64OpcodeInfo &;

/**
 * @brief A single AArch64 machine instruction (generic: opcode + operands, form
 * from the opcode table). Carries an optional condition code for cset / b.<cond>
 * and an optional trailing "// ..." annotation.
 */
class AArch64Inst : public TargetInst {
 public:
  explicit AArch64Inst(A64Opcode opcode) : opcode_(opcode) {}

  auto GetOpcode() const -> A64Opcode { return opcode_; }
  auto SetOpcode(A64Opcode opcode) -> void { opcode_ = opcode; }

  auto GetCond() const -> A64Cond { return cond_; }
  auto SetCond(A64Cond cond) -> void { cond_ = cond; }

  auto SetComment(std::string comment) -> void { comment_ = std::move(comment); }
  auto GetComment() const -> const std::string & { return comment_; }

  auto Emit(std::ofstream *out) -> void override;

 private:
  A64Opcode opcode_;
  A64Cond cond_ = A64Cond::None;
  std::string comment_;
};

}  // namespace backend::targetir::aarch64

#endif
