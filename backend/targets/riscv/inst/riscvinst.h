#ifndef BACKEND_TARGETS_RISCV_INST_RISCVINST_H
#define BACKEND_TARGETS_RISCV_INST_RISCVINST_H

#include <cstdint>
#include <fstream>
#include <string>
#include <utility>
#include "backend/targets/utils/inst/targetinst.h"

namespace backend::targetir::riscv {

/**
 * @brief RISC-V (rv64) opcodes handled by the backend. int operations use the
 * 32-bit .w variants so SysY 32-bit wrap-around semantics are preserved;
 * 64-bit add/sub are reserved for address arithmetic. Extend as milestones add
 * float / wider addressing.
 */
enum class RVOpcode : uint8_t {
  // R-type integer (rd, rs1, rs2)
  ADDW,
  SUBW,
  MULW,
  DIVW,
  REMW,
  ADD,
  SUB,
  MUL,  // 64-bit, address arithmetic
  AND,
  OR,
  XOR,
  SLLW,
  SRLW,
  SRAW,
  SLT,
  SLTU,
  // I-type integer (rd, rs1, imm)
  ADDIW,
  ADDI,
  ANDI,
  ORI,
  XORI,
  SLLIW,
  SRLIW,
  SRAIW,
  SLLI,  // 64-bit shift-left immediate (address arithmetic / strength reduction)
  SLTI,
  SLTIU,
  // Unary (rd, rs1)
  MV,
  NEGW,
  SEQZ,
  SNEZ,
  SEXT_W,
  // Immediate / address materialization (rd, imm|symbol)
  LI,
  LA,
  // Memory
  LW,
  LD,  // load  : rd, imm(rs1)
  SW,
  SD,  // store : rs2, imm(rs1)
  // Single-precision float
  FADD_S,
  FSUB_S,
  FMUL_S,
  FDIV_S,  // R-type, FPR operands + FPR result
  FEQ_S,
  FLT_S,
  FLE_S,     // R-type, FPR operands -> GPR result
  FMV_S,     // fmv.s  fd, fs   (float register move)
  FCVT_S_W,  // fcvt.s.w fd, rs (int -> float)
  FCVT_W_S,  // fcvt.w.s rd, fs, rtz (float -> int, truncate)
  FMV_W_X,
  FMV_X_W,  // bit-pattern move GPR<->FPR
  FLW,
  FSW,  // float load / store
  // Control flow
  BEQ,
  BNE,
  BLT,
  BGE,
  BLTU,
  BGEU,     // rs1, rs2, label
  J,        // label
  CALL,     // symbol
  RET,      // (none)
  COMMENT,  // "# text" pseudo, carries no operands; used for annotations / TODOs
  SRAI,     // 64-bit arithmetic shift-right immediate (magic-division high part);
            // kept out of the index-aligned table like CALL/RET
};

/// How an opcode's operands are rendered as text.
enum class RVForm : uint8_t {
  RType,         // mnem rd, op0, op1
  IType,         // mnem rd, op0, imm
  Unary,         // mnem rd, op0
  Load,          // mnem rd, op1(op0)        op0 = base, op1 = imm
  Store,         // mnem op0, op2(op1)       op0 = value, op1 = base, op2 = imm
  Branch,        // mnem op0, op1, op2       op0/op1 = regs, op2 = label
  Jump,          // mnem op0
  LoadImm,       // mnem rd, op0             op0 = imm
  LoadSym,       // mnem rd, op0             op0 = symbol
  Call,          // mnem op0                 op0 = symbol
  Ret,           // mnem
  ConvertToInt,  // mnem rd, op0, rtz  (fcvt.w.s truncating convert)
};

struct RVOpcodeInfo {
  const char *mnemonic_;
  RVForm form_;
};

/// The mnemonic + rendering form for an opcode.
auto GetRVOpcodeInfo(RVOpcode opcode) -> const RVOpcodeInfo &;

/**
 * @brief A single RISC-V machine instruction. Generic (opcode + operands) rather
 * than a class-per-instruction: the printing form comes from the opcode table,
 * which keeps instruction selection and later passes uniform and terse.
 */
class RISCVInst : public TargetInst {
 public:
  explicit RISCVInst(RVOpcode opcode) : opcode_(opcode) {}

  auto GetOpcode() const -> RVOpcode { return opcode_; }
  // Rewrite the opcode in place, used by machine peephole passes (e.g. turning
  // `mul` by a power of two into `slli`, or an R-type op into its I-type form).
  auto SetOpcode(RVOpcode opcode) -> void { opcode_ = opcode; }

  // Optional trailing "# ..." annotation; on a COMMENT opcode it is the whole line.
  auto SetComment(std::string comment) -> void { comment_ = std::move(comment); }
  auto GetComment() const -> const std::string & { return comment_; }

  auto Emit(std::ofstream *out) -> void override;

 private:
  RVOpcode opcode_;
  std::string comment_;
};

}  // namespace backend::targetir::riscv

#endif
