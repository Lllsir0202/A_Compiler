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

auto ARMMovzInst::Emit(std::ofstream *out) -> void {
  *out << "    movz ";
  result_->Emit(out);
  *out << ", #";
  operands_[0]->Emit(out);
  if (shift_ != 0) {
    *out << ", LSL #" << shift_;
  }
  *out << "\n";
}

auto ARMMovzInst::Create(TargetOperand *dst, TargetOperand *imm, int shift) -> ARMMovzInst * {
  auto *inst = new ARMMovzInst();
  inst->operands_.reserve(1);
  inst->AddOperand(imm);
  inst->shift_ = shift;
  inst->SetResult(dst);
  return inst;
}

auto ARMMovkInst::Emit(std::ofstream *out) -> void {
  *out << "    movk ";
  result_->Emit(out);
  *out << ", #";
  operands_[0]->Emit(out);
  if (shift_ != 0) {
    *out << ", LSL #" << shift_;
  }
  *out << "\n";
}

auto ARMMovkInst::Create(TargetOperand *dst, TargetOperand *imm, int shift) -> ARMMovkInst * {
  auto *inst = new ARMMovkInst();
  inst->operands_.reserve(1);
  inst->AddOperand(imm);
  inst->shift_ = shift;
  inst->SetResult(dst);
  return inst;
}

auto ARMMovnInst::Emit(std::ofstream *out) -> void {
  *out << "    movn ";
  result_->Emit(out);
  *out << ", #";
  operands_[0]->Emit(out);
  if (shift_ != 0) {
    *out << ", LSL #" << shift_;
  }
  *out << "\n";
}

auto ARMMovnInst::Create(TargetOperand *dst, TargetOperand *imm, int shift) -> ARMMovnInst * {
  auto *inst = new ARMMovnInst();
  inst->operands_.reserve(1);
  inst->AddOperand(imm);
  inst->shift_ = shift;
  inst->SetResult(dst);
  return inst;
}

}  // namespace backend::targetir::aarch64