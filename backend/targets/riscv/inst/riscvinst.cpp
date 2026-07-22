#include "backend/targets/riscv/inst/riscvinst.h"
#include <array>
#include "backend/targets/utils/operand/targetoperand.h"

namespace backend::targetir::riscv {

auto GetRVOpcodeInfo(RVOpcode opcode) -> const RVOpcodeInfo & {
  static const std::array<RVOpcodeInfo, 60> TABLE = {{
      {"addw", RVForm::RType},    {"subw", RVForm::RType},     {"mulw", RVForm::RType},
      {"divw", RVForm::RType},    {"remw", RVForm::RType},     {"add", RVForm::RType},
      {"sub", RVForm::RType},     {"mul", RVForm::RType},      {"and", RVForm::RType},
      {"or", RVForm::RType},      {"xor", RVForm::RType},      {"sllw", RVForm::RType},
      {"srlw", RVForm::RType},    {"sraw", RVForm::RType},     {"slt", RVForm::RType},
      {"sltu", RVForm::RType},    {"addiw", RVForm::IType},    {"addi", RVForm::IType},
      {"andi", RVForm::IType},    {"ori", RVForm::IType},      {"xori", RVForm::IType},
      {"slliw", RVForm::IType},   {"srliw", RVForm::IType},    {"sraiw", RVForm::IType},
      {"slli", RVForm::IType},    {"slti", RVForm::IType},     {"sltiu", RVForm::IType},
      {"mv", RVForm::Unary},      {"negw", RVForm::Unary},     {"seqz", RVForm::Unary},
      {"snez", RVForm::Unary},    {"sext.w", RVForm::Unary},   {"li", RVForm::LoadImm},
      {"la", RVForm::LoadSym},    {"lw", RVForm::Load},        {"ld", RVForm::Load},
      {"sw", RVForm::Store},      {"sd", RVForm::Store},       {"fadd.s", RVForm::RType},
      {"fsub.s", RVForm::RType},  {"fmul.s", RVForm::RType},   {"fdiv.s", RVForm::RType},
      {"feq.s", RVForm::RType},   {"flt.s", RVForm::RType},    {"fle.s", RVForm::RType},
      {"fmv.s", RVForm::Unary},   {"fcvt.s.w", RVForm::Unary}, {"fcvt.w.s", RVForm::ConvertToInt},
      {"fmv.w.x", RVForm::Unary}, {"fmv.x.w", RVForm::Unary},  {"flw", RVForm::Load},
      {"fsw", RVForm::Store},     {"beq", RVForm::Branch},     {"bne", RVForm::Branch},
      {"blt", RVForm::Branch},    {"bge", RVForm::Branch},     {"bltu", RVForm::Branch},
      {"bgeu", RVForm::Branch},   {"j", RVForm::Jump},
  }};
  // CALL and RET have no operands-based form; handle them out of the table so the
  // table stays index-aligned with the RVOpcode enum up to J.
  static const RVOpcodeInfo CALL_INFO = {"call", RVForm::Call};
  static const RVOpcodeInfo RET_INFO = {"ret", RVForm::Ret};
  static const RVOpcodeInfo SRAI_INFO = {"srai", RVForm::IType};
  if (opcode == RVOpcode::CALL) {
    return CALL_INFO;
  }
  if (opcode == RVOpcode::RET) {
    return RET_INFO;
  }
  if (opcode == RVOpcode::SRAI) {
    return SRAI_INFO;
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
  }
  if (!comment_.empty()) {
    *out << "  # " << comment_;
  }
  *out << "\n";
}

}  // namespace backend::targetir::riscv
