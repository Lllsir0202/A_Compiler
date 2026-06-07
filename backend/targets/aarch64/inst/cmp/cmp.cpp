#include "backend/targets/aarch64/inst/cmp/cmp.h"
#include "backend/targets/utils/operand/targetoperand.h"

namespace backend::targetir::aarch64 {
auto ARMCMPInst::Emit(std::ofstream *out) -> void {
  *out << "    cmp ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << "\n";
}

auto ARMCMPInst::Create(TargetOperand *rs1, TargetOperand *rs2) -> ARMCMPInst * {
  auto *inst = new ARMCMPInst();
  inst->operands_.reserve(2);
  inst->operands_.emplace_back(rs1);
  inst->operands_.emplace_back(rs2);
  return inst;
}

auto ARMFCMPInst::Emit(std::ofstream *out) -> void {
  *out << "    fcmp ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << "\n";
}

auto ARMFCMPInst::Create(TargetOperand *rs1, TargetOperand *imm) -> ARMFCMPInst * {
  auto *inst = new ARMFCMPInst();
  inst->operands_.reserve(2);
  inst->operands_.emplace_back(rs1);
  inst->operands_.emplace_back(imm);
  return inst;
}

auto ARMCSETInst::Emit(std::ofstream *out) -> void {
  *out << "    cset ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << "\n";
}

auto ARMCSETInst::Create(TargetOperand *rd, TargetOperand *cond) -> ARMCSETInst * {
  auto *inst = new ARMCSETInst();
  inst->operands_.reserve(1);
  inst->operands_.emplace_back(cond);
  inst->SetResult(rd);
  return inst;
}

}  // namespace backend::targetir::aarch64