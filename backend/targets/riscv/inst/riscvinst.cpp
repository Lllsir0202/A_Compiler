#include "backend/targets/riscv/inst/riscvinst.h"
#include <array>
#include "backend/targets/utils/operand/targetoperand.h"

namespace backend::targetir::riscv {

auto GetRVOpcodeInfo(RVOpcode opcode) -> const RVOpcodeInfo & {
  constexpr auto N = RVInstFlag::None;
  constexpr auto C = RVInstFlag::Commutable;
  constexpr auto R = RVInstFlag::Rematerializable;
  constexpr auto LD = RVInstFlag::MayLoad;
  constexpr auto ST = RVInstFlag::MayStore | RVInstFlag::SideEffect;
  constexpr auto BR = RVInstFlag::Terminator | RVInstFlag::Branch | RVInstFlag::SideEffect;
  static const std::array<RVOpcodeInfo, 60> TABLE = {{
      {"addw", RVForm::RType, 2, true, C},     {"subw", RVForm::RType, 2, true, N},
      {"mulw", RVForm::RType, 2, true, C},     {"divw", RVForm::RType, 2, true, N},
      {"remw", RVForm::RType, 2, true, N},     {"add", RVForm::RType, 2, true, C},
      {"sub", RVForm::RType, 2, true, N},      {"mul", RVForm::RType, 2, true, C},
      {"and", RVForm::RType, 2, true, C},      {"or", RVForm::RType, 2, true, C},
      {"xor", RVForm::RType, 2, true, C},      {"sllw", RVForm::RType, 2, true, N},
      {"srlw", RVForm::RType, 2, true, N},     {"sraw", RVForm::RType, 2, true, N},
      {"slt", RVForm::RType, 2, true, N},      {"sltu", RVForm::RType, 2, true, N},
      {"addiw", RVForm::IType, 2, true, N},    {"addi", RVForm::IType, 2, true, N},
      {"andi", RVForm::IType, 2, true, N},     {"ori", RVForm::IType, 2, true, N},
      {"xori", RVForm::IType, 2, true, N},     {"slliw", RVForm::IType, 2, true, N},
      {"srliw", RVForm::IType, 2, true, N},    {"sraiw", RVForm::IType, 2, true, N},
      {"slli", RVForm::IType, 2, true, N},     {"slti", RVForm::IType, 2, true, N},
      {"sltiu", RVForm::IType, 2, true, N},    {"mv", RVForm::Unary, 1, true, N},
      {"negw", RVForm::Unary, 1, true, N},     {"seqz", RVForm::Unary, 1, true, N},
      {"snez", RVForm::Unary, 1, true, N},     {"sext.w", RVForm::Unary, 1, true, N},
      {"li", RVForm::LoadImm, 1, true, R},     {"la", RVForm::LoadSym, 1, true, R},
      {"lw", RVForm::Load, 2, true, LD},       {"ld", RVForm::Load, 2, true, LD},
      {"sw", RVForm::Store, 3, false, ST},     {"sd", RVForm::Store, 3, false, ST},
      {"fadd.s", RVForm::RType, 2, true, C},   {"fsub.s", RVForm::RType, 2, true, N},
      {"fmul.s", RVForm::RType, 2, true, C},   {"fdiv.s", RVForm::RType, 2, true, N},
      {"feq.s", RVForm::RType, 2, true, C},    {"flt.s", RVForm::RType, 2, true, N},
      {"fle.s", RVForm::RType, 2, true, N},    {"fmv.s", RVForm::Unary, 1, true, N},
      {"fcvt.s.w", RVForm::Unary, 1, true, N}, {"fcvt.w.s", RVForm::ConvertToInt, 1, true, N},
      {"fmv.w.x", RVForm::Unary, 1, true, N},  {"fmv.x.w", RVForm::Unary, 1, true, N},
      {"flw", RVForm::Load, 2, true, LD},      {"fsw", RVForm::Store, 3, false, ST},
      {"beq", RVForm::Branch, 3, false, BR},   {"bne", RVForm::Branch, 3, false, BR},
      {"blt", RVForm::Branch, 3, false, BR},   {"bge", RVForm::Branch, 3, false, BR},
      {"bltu", RVForm::Branch, 3, false, BR},  {"bgeu", RVForm::Branch, 3, false, BR},
      {"j", RVForm::Jump, 1, false, BR},
  }};
  // CALL, RET, COMMENT and SRAI sit after the index-aligned table.
  static const RVOpcodeInfo CALL_INFO = {
      "call", RVForm::Call, -1, false,
      RVInstFlag::Call | RVInstFlag::MayLoad | RVInstFlag::MayStore | RVInstFlag::SideEffect};
  static const RVOpcodeInfo RET_INFO = {"ret", RVForm::Ret, 0, false,
                                        RVInstFlag::Terminator | RVInstFlag::Return | RVInstFlag::SideEffect};
  static const RVOpcodeInfo COMMENT_INFO = {"#", RVForm::Ret, 0, false, N};
  static const RVOpcodeInfo SRAI_INFO = {"srai", RVForm::IType, 2, true, N};
  static const RVOpcodeInfo PHI_INFO = {"phi", RVForm::Phi, -1, true, RVInstFlag::Pseudo};
  if (opcode == RVOpcode::CALL) {
    return CALL_INFO;
  }
  if (opcode == RVOpcode::RET) {
    return RET_INFO;
  }
  if (opcode == RVOpcode::COMMENT) {
    return COMMENT_INFO;
  }
  if (opcode == RVOpcode::SRAI) {
    return SRAI_INFO;
  }
  if (opcode == RVOpcode::PHI) {
    return PHI_INFO;
  }
  return TABLE[static_cast<size_t>(opcode)];
}

auto RISCVInst::Emit(std::ofstream *out) -> void {
  if (opcode_ == RVOpcode::COMMENT) {
    *out << "\t# " << comment_ << "\n";
    return;
  }
  const auto &info = GetRVOpcodeInfo(opcode_);
  *out << "\t" << info.mnemonic_;
  const auto &ops = operands_;
  switch (info.form_) {
    case RVForm::RType:
      *out << " ";
      result_->Emit(out);
      *out << ", ";
      ops[0]->Emit(out);
      *out << ", ";
      ops[1]->Emit(out);
      break;
    case RVForm::IType:
      *out << " ";
      result_->Emit(out);
      *out << ", ";
      ops[0]->Emit(out);
      *out << ", ";
      ops[1]->Emit(out);
      break;
    case RVForm::Unary:
      *out << " ";
      result_->Emit(out);
      *out << ", ";
      ops[0]->Emit(out);
      break;
    case RVForm::Load:
      *out << " ";
      result_->Emit(out);
      *out << ", ";
      ops[1]->Emit(out);
      *out << "(";
      ops[0]->Emit(out);
      *out << ")";
      break;
    case RVForm::Store:
      *out << " ";
      ops[0]->Emit(out);
      *out << ", ";
      ops[2]->Emit(out);
      *out << "(";
      ops[1]->Emit(out);
      *out << ")";
      break;
    case RVForm::Branch:
      *out << " ";
      ops[0]->Emit(out);
      *out << ", ";
      ops[1]->Emit(out);
      *out << ", ";
      ops[2]->Emit(out);
      break;
    case RVForm::LoadImm:
    case RVForm::LoadSym:
      *out << " ";
      result_->Emit(out);
      *out << ", ";
      ops[0]->Emit(out);
      break;
    case RVForm::Jump:
    case RVForm::Call:
      *out << " ";
      ops[0]->Emit(out);
      break;
    case RVForm::ConvertToInt:  // fcvt.w.s rd, fs, rtz
      *out << " ";
      result_->Emit(out);
      *out << ", ";
      ops[0]->Emit(out);
      *out << ", rtz";
      break;
    case RVForm::Ret:
      break;
    case RVForm::Phi:
      *out << " # unlowered machine phi";
      break;
  }
  if (!comment_.empty()) {
    *out << "  # " << comment_;
  }
  *out << "\n";
}

}  // namespace backend::targetir::riscv
