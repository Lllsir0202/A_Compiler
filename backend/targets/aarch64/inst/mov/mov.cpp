#include "backend/targets/aarch64/inst/mov/mov.h"
#include "backend/targets/utils/operand/targetoperand.h"

namespace backend::targetir::aarch64 {
// Move instruction for register
auto ARMMovInst::Emit(std::ofstream *out) -> void {
  *out << "    mov ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << "\n";
}

auto ARMMovInst::Create(TargetOperand *dst, TargetOperand *src) -> ARMMovInst * {
  auto *inst = new ARMMovInst();
  inst->operands_.reserve(1);
  inst->AddOperand(src);
  inst->SetResult(dst);
  return inst;
}

// Move instruction for immediate value
auto ARMMoviInst::Emit(std::ofstream *out) -> void {
  *out << "    mov ";
  result_->Emit(out);
  *out << ", #";
  operands_[0]->Emit(out);
  *out << "\n";
}

auto ARMMoviInst::Create(TargetOperand *dst, TargetOperand *imm) -> ARMMoviInst * {
  auto *inst = new ARMMoviInst();
  inst->operands_.reserve(1);
  inst->AddOperand(imm);
  inst->SetResult(dst);
  return inst;
}

auto ARMFMovInst::Emit(std::ofstream *out) -> void {
  *out << "    fmov ";
  result_->Emit(out);
  *out << ", ";
  operands_[0]->Emit(out);
  *out << "\n";
}

auto ARMFMovInst::Create(TargetOperand *dst, TargetOperand *src) -> ARMFMovInst * {
  auto *inst = new ARMFMovInst();
  inst->operands_.reserve(1);
  inst->AddOperand(src);
  inst->SetResult(dst);
  return inst;
}

}  // namespace backend::targetir::aarch64