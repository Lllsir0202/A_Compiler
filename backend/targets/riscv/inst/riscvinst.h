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
  ADDW, SUBW, MULW, DIVW, REMW,
  ADD, SUB, MUL,  // 64-bit, address arithmetic
  AND, OR, XOR,
  SLLW, SRLW, SRAW,
  SLT, SLTU,
  // I-type integer (rd, rs1, imm)
  ADDIW, ADDI, ANDI, ORI, XORI,
  SLLIW, SRLIW, SRAIW,
  SLTI, SLTIU,
  // Unary (rd, rs1)
  MV, NEGW, SEQZ, SNEZ, SEXT_W,
  // Immediate / address materialization (rd, imm|symbol)
  LI, LA,
  // Memory
  LW, LD,   // load  : rd, imm(rs1)
  SW, SD,   // store : rs2, imm(rs1)
  // Control flow
  BEQ, BNE, BLT, BGE, BLTU, BGEU,  // rs1, rs2, label
  J,        // label
  CALL,     // symbol
  RET,      // (none)
  COMMENT,  // "# text" pseudo, carries no operands; used for annotations / TODOs
};

/// How an opcode's operands are rendered as text.
enum class RVForm : uint8_t {
  RType,   // mnem rd, op0, op1
  IType,   // mnem rd, op0, imm
  Unary,   // mnem rd, op0
  Load,    // mnem rd, op1(op0)        op0 = base, op1 = imm
  Store,   // mnem op0, op2(op1)       op0 = value, op1 = base, op2 = imm
  Branch,  // mnem op0, op1, op2       op0/op1 = regs, op2 = label
  Jump,    // mnem op0
  LoadImm, // mnem rd, op0             op0 = imm
  LoadSym, // mnem rd, op0             op0 = symbol
  Call,    // mnem op0                 op0 = symbol
  Ret,     // mnem
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
