#include "backend/targets/aarch64/inst/aarch64inst.h"
#include <array>
#include "backend/targets/utils/operand/targetoperand.h"

namespace backend::targetir::aarch64 {

auto A64CondName(A64Cond cond) -> const char * {
  switch (cond) {
    case A64Cond::EQ:
      return "eq";
    case A64Cond::NE:
      return "ne";
    case A64Cond::LT:
      return "lt";
    case A64Cond::LE:
      return "le";
    case A64Cond::GT:
      return "gt";
    case A64Cond::GE:
      return "ge";
    default:
      return "al";
  }
}

auto GetA64OpcodeInfo(A64Opcode opcode) -> const A64OpcodeInfo & {
  using F = A64Form;
  static const std::array<A64OpcodeInfo, 50> TABLE = {{
      {"add", F::R3},       {"sub", F::R3},       {"mul", F::R3},        {"sdiv", F::R3},      {"and", F::R3},
      {"orr", F::R3},       {"eor", F::R3},       {"lsl", F::R3},        {"lsr", F::R3},       {"asr", F::R3},
      {"msub", F::R4},      {"madd", F::R4},      {"add", F::RI},        {"sub", F::RI},       {"and", F::RI},
      {"orr", F::RI},       {"eor", F::RI},       {"lsl", F::RI},        {"lsr", F::RI},       {"asr", F::RI},
      {"mov", F::MovReg},   {"movz", F::MovImm},  {"movn", F::MovImm},   {"movk", F::MovK},    {"sxtw", F::Unary},
      {"cmp", F::Cmp},      {"cmp", F::CmpImm},   {"cset", F::Cset},     {"b", F::BCond},      {"b", F::Branch},
      {"cbz", F::CbzForm},  {"cbnz", F::CbzForm}, {"ldr", F::LoadOff},   {"str", F::StoreOff}, {"ldr", F::LoadReg},
      {"str", F::StoreReg}, {"adrp", F::Adrp},    {"add", F::AddLo12},   {"fadd", F::R3},      {"fsub", F::R3},
      {"fmul", F::R3},      {"fdiv", F::R3},      {"fcmp", F::FCmpForm}, {"fmov", F::MovReg},  {"fmov", F::Unary},
      {"fmov", F::Unary},   {"scvtf", F::Unary},  {"fcvtzs", F::Unary},  {"bl", F::Call},      {"ret", F::Ret},
  }};
  return TABLE[static_cast<size_t>(opcode)];
}

auto AArch64Inst::Emit(std::ofstream *out) -> void {
  if (opcode_ == A64Opcode::COMMENT) {
    *out << "\t// " << comment_ << "\n";
    return;
  }
  if (opcode_ == A64Opcode::SMULH) {
    // High 32 bits of the signed 32x32 product, via the reserved x16 scratch.
    *out << "\tsmull x16, ";
    operands_[0]->Emit(out);
    *out << ", ";
    operands_[1]->Emit(out);
    *out << "\n\tasr x16, x16, #32\n\tmov ";
    result_->Emit(out);
    *out << ", w16\n";
    return;
  }
  if (opcode_ == A64Opcode::CSEL) {
    // rd = cond ? rn : rm.  operands {rn, rm}, condition in cond_.
    *out << "\tcsel ";
    result_->Emit(out);
    *out << ", ";
    operands_[0]->Emit(out);
    *out << ", ";
    operands_[1]->Emit(out);
    *out << ", " << A64CondName(cond_) << "\n";
    return;
  }
  if (opcode_ == A64Opcode::STP || opcode_ == A64Opcode::LDP) {
    // {rt1, rt2, base, imm} -> stp/ldp rt1, rt2, [base, #imm].
    *out << (opcode_ == A64Opcode::STP ? "\tstp " : "\tldp ");
    operands_[0]->Emit(out);
    *out << ", ";
    operands_[1]->Emit(out);
    *out << ", [";
    operands_[2]->Emit(out);
    *out << ", #";
    operands_[3]->Emit(out);
    *out << "]\n";
    return;
  }
  const auto &info = GetA64OpcodeInfo(opcode_);
  const auto &ops = operands_;
  auto imm = [&](TargetOperand *o) {
    *out << "#";
    o->Emit(out);
  };

  *out << "\t" << info.mnemonic_;
  switch (info.form_) {
    case A64Form::R3:
      *out << " ";
      result_->Emit(out);
      *out << ", ";
      ops[0]->Emit(out);
      *out << ", ";
      ops[1]->Emit(out);
      break;
    case A64Form::R4:
      *out << " ";
      result_->Emit(out);
      *out << ", ";
      ops[0]->Emit(out);
      *out << ", ";
      ops[1]->Emit(out);
      *out << ", ";
      ops[2]->Emit(out);
      break;
    case A64Form::RI:
      *out << " ";
      result_->Emit(out);
      *out << ", ";
      ops[0]->Emit(out);
      *out << ", ";
      imm(ops[1]);
      break;
    case A64Form::MovReg:
    case A64Form::Unary:
      *out << " ";
      result_->Emit(out);
      *out << ", ";
      ops[0]->Emit(out);
      break;
    case A64Form::MovImm:
      *out << " ";
      result_->Emit(out);
      *out << ", ";
      imm(ops[0]);
      break;
    case A64Form::MovK:
      // ops[0] is the read of the destination (movk is read-modify-write, kept so
      // the allocator ties it to the movz); the printed immediate/shift are [1],[2].
      *out << " ";
      result_->Emit(out);
      *out << ", ";
      imm(ops[1]);
      *out << ", lsl ";
      imm(ops[2]);
      break;
    case A64Form::Cmp:
      *out << " ";
      ops[0]->Emit(out);
      *out << ", ";
      ops[1]->Emit(out);
      break;
    case A64Form::CmpImm:
      *out << " ";
      ops[0]->Emit(out);
      *out << ", ";
      imm(ops[1]);
      break;
    case A64Form::Cset:
      *out << " ";
      result_->Emit(out);
      *out << ", " << A64CondName(cond_);
      break;
    case A64Form::BCond:
      *out << "." << A64CondName(cond_) << " ";
      ops[0]->Emit(out);
      break;
    case A64Form::Branch:
      *out << " ";
      ops[0]->Emit(out);
      break;
    case A64Form::CbzForm:
      *out << " ";
      ops[0]->Emit(out);
      *out << ", ";
      ops[1]->Emit(out);
      break;
    case A64Form::LoadOff:
      *out << " ";
      result_->Emit(out);
      *out << ", [";
      ops[0]->Emit(out);
      *out << ", ";
      imm(ops[1]);
      *out << "]";
      break;
    case A64Form::StoreOff:
      *out << " ";
      ops[0]->Emit(out);
      *out << ", [";
      ops[1]->Emit(out);
      *out << ", ";
      imm(ops[2]);
      *out << "]";
      break;
    case A64Form::LoadReg:
      *out << " ";
      result_->Emit(out);
      *out << ", [";
      ops[0]->Emit(out);
      *out << ", ";
      ops[1]->Emit(out);
      *out << ", lsl ";
      imm(ops[2]);
      *out << "]";
      break;
    case A64Form::StoreReg:
      *out << " ";
      ops[0]->Emit(out);
      *out << ", [";
      ops[1]->Emit(out);
      *out << ", ";
      ops[2]->Emit(out);
      *out << ", lsl ";
      imm(ops[3]);
      *out << "]";
      break;
    case A64Form::Adrp:
      *out << " ";
      result_->Emit(out);
      *out << ", ";
      ops[0]->Emit(out);
      break;
    case A64Form::AddLo12:
      *out << " ";
      result_->Emit(out);
      *out << ", ";
      ops[0]->Emit(out);
      *out << ", :lo12:";
      ops[1]->Emit(out);
      break;
    case A64Form::FCmpForm:
      *out << " ";
      ops[0]->Emit(out);
      *out << ", ";
      ops[1]->Emit(out);
      break;
    case A64Form::Call:
      *out << " ";
      ops[0]->Emit(out);
      break;
    case A64Form::Ret:
      break;
  }
  if (!comment_.empty()) {
    *out << "  // " << comment_;
  }
  *out << "\n";
}

}  // namespace backend::targetir::aarch64
