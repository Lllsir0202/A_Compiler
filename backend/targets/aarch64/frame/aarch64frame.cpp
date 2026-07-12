#include "backend/targets/aarch64/frame/aarch64frame.h"
#include <cstdint>
#include "backend/targets/aarch64/regs/aarch64reg.h"
#include "backend/targets/utils/operand/targetreg.h"
#include "backend/targets/utils/struct/targetblock.h"
#include "backend/targets/utils/struct/targetfunction.h"

namespace backend::targetir::aarch64 {

namespace {
constexpr auto SP = static_cast<uint8_t>(GPR::Sp);
constexpr auto FP = static_cast<uint8_t>(GPR::Fp);
constexpr auto LR = static_cast<uint8_t>(GPR::Lr);
constexpr auto X16 = static_cast<uint8_t>(GPR::X16);
constexpr int IMM12_MAX = 4095;

auto AlignUp16(size_t n) -> size_t { return (n + 15U) & ~static_cast<size_t>(15U); }
}  // namespace

// Materialize `offset` into x16 and return x16 = sp + offset.
auto AArch64Frame::MatOffset(std::vector<TargetInst *> *out, int offset) -> TargetOperand * {
  auto o = static_cast<uint32_t>(offset);
  auto *x16 = ctx_->PhysReg(TargetRegClass::General, TargetRegSize::QWord, X16);
  auto *sp = ctx_->PhysReg(TargetRegClass::General, TargetRegSize::QWord, SP);
  auto *mz = ctx_->CreateInst<AArch64Inst>(A64Opcode::MOVZ);
  mz->SetResult(x16);
  mz->AddOperand(ctx_->Imm(static_cast<int>(o & 0xFFFFU)));
  out->push_back(mz);
  if ((o >> 16U) != 0) {
    auto *mk = ctx_->CreateInst<AArch64Inst>(A64Opcode::MOVK);
    mk->SetResult(x16);
    mk->AddOperand(x16);
    mk->AddOperand(ctx_->Imm(static_cast<int>((o >> 16U) & 0xFFFFU)));
    mk->AddOperand(ctx_->Imm(16));
    out->push_back(mk);
  }
  auto *add = ctx_->CreateInst<AArch64Inst>(A64Opcode::ADD);
  add->SetResult(x16);
  add->AddOperand(sp);
  add->AddOperand(x16);
  out->push_back(add);
  return x16;
}

auto AArch64Frame::MakeAddSp(int delta) -> std::vector<TargetInst *> {
  std::vector<TargetInst *> seq;
  auto *sp = ctx_->PhysReg(TargetRegClass::General, TargetRegSize::QWord, SP);
  int mag = delta < 0 ? -delta : delta;
  if (mag <= IMM12_MAX) {
    auto *inst = ctx_->CreateInst<AArch64Inst>(delta < 0 ? A64Opcode::SUB_I : A64Opcode::ADD_I);
    inst->SetResult(sp);
    inst->AddOperand(sp);
    inst->AddOperand(ctx_->Imm(mag));
    seq.push_back(inst);
    return seq;
  }
  // Out of range: build the magnitude in x16 and add/sub it.
  auto o = static_cast<uint32_t>(mag);
  auto *x16 = ctx_->PhysReg(TargetRegClass::General, TargetRegSize::QWord, X16);
  auto *mz = ctx_->CreateInst<AArch64Inst>(A64Opcode::MOVZ);
  mz->SetResult(x16);
  mz->AddOperand(ctx_->Imm(static_cast<int>(o & 0xFFFFU)));
  seq.push_back(mz);
  if ((o >> 16U) != 0) {
    auto *mk = ctx_->CreateInst<AArch64Inst>(A64Opcode::MOVK);
    mk->SetResult(x16);
    mk->AddOperand(x16);
    mk->AddOperand(ctx_->Imm(static_cast<int>((o >> 16U) & 0xFFFFU)));
    mk->AddOperand(ctx_->Imm(16));
    seq.push_back(mk);
  }
  auto *op = ctx_->CreateInst<AArch64Inst>(delta < 0 ? A64Opcode::SUB : A64Opcode::ADD);
  op->SetResult(sp);
  op->AddOperand(sp);
  op->AddOperand(x16);
  seq.push_back(op);
  return seq;
}

auto AArch64Frame::MakeSetFp(int frame) -> std::vector<TargetInst *> {
  std::vector<TargetInst *> seq;
  auto *sp = ctx_->PhysReg(TargetRegClass::General, TargetRegSize::QWord, SP);
  auto *fp = ctx_->PhysReg(TargetRegClass::General, TargetRegSize::QWord, FP);
  if (frame <= IMM12_MAX) {
    auto *add = ctx_->CreateInst<AArch64Inst>(A64Opcode::ADD_I);
    add->SetResult(fp);
    add->AddOperand(sp);
    add->AddOperand(ctx_->Imm(frame));
    seq.push_back(add);
    return seq;
  }
  auto *x16 = MatOffset(&seq, frame);
  auto *add = ctx_->CreateInst<AArch64Inst>(A64Opcode::MOV);
  add->SetResult(fp);
  add->AddOperand(x16);
  seq.push_back(add);
  return seq;
}

auto AArch64Frame::EmitStackAccess(std::vector<TargetInst *> *out, A64Opcode op, TargetOperand *reg, int offset)
    -> void {
  auto *sp = ctx_->PhysReg(TargetRegClass::General, TargetRegSize::QWord, SP);
  bool is_load = op == A64Opcode::LDR_OFF;
  auto build = [&](TargetOperand *base, int imm) {
    auto *inst = ctx_->CreateInst<AArch64Inst>(op);
    if (is_load) {
      inst->SetResult(reg);
      inst->AddOperand(base);
      inst->AddOperand(ctx_->Imm(imm));
    } else {
      inst->AddOperand(reg);
      inst->AddOperand(base);
      inst->AddOperand(ctx_->Imm(imm));
    }
    out->push_back(inst);
  };
  if (offset >= 0 && offset <= IMM12_MAX) {
    build(sp, offset);
    return;
  }
  build(MatOffset(out, offset), 0);
}

auto AArch64Frame::Finalize(TargetFunction *func, size_t locals_bytes, size_t spill_slots,
                            const std::vector<std::pair<TargetRegClass, uint8_t>> &callee_saved) -> void {
  const auto &blocks = func->GetBlocks();
  if (blocks.empty()) {
    return;
  }
  size_t cs_base = locals_bytes + spill_slots * 8;
  auto frame = static_cast<int>(AlignUp16(cs_base + callee_saved.size() * 8 + 16));
  int fp_off = frame - 16;
  int lr_off = frame - 8;

  auto *fp = ctx_->PhysReg(TargetRegClass::General, TargetRegSize::QWord, FP);
  auto *lr = ctx_->PhysReg(TargetRegClass::General, TargetRegSize::QWord, LR);

  // Callee-saved registers are saved at 64-bit width (x-view for GPR, d-view for
  // FPR: only d8..d15 are callee-saved, and the single-precision value lives in
  // the low half).
  auto cs_reg = [&](TargetRegClass cls, uint8_t id) {
    return ctx_->PhysReg(cls, TargetRegSize::QWord, id);
  };

  // Prologue.
  std::vector<TargetInst *> prologue = MakeAddSp(-frame);
  EmitStackAccess(&prologue, A64Opcode::STR_OFF, fp, fp_off);
  EmitStackAccess(&prologue, A64Opcode::STR_OFF, lr, lr_off);
  for (auto *inst : MakeSetFp(frame)) {
    prologue.push_back(inst);
  }
  for (size_t i = 0; i < callee_saved.size(); ++i) {
    auto [cls, id] = callee_saved[i];
    EmitStackAccess(&prologue, A64Opcode::STR_OFF, cs_reg(cls, id), static_cast<int>(cs_base + i * 8));
  }

  auto *entry = blocks.front();
  std::vector<TargetInst *> new_entry = prologue;
  for (auto *inst : entry->GetInsts()) {
    new_entry.push_back(inst);
  }
  entry->SetInsts(new_entry);

  // Epilogue: expand each `ret`.
  for (auto *block : blocks) {
    std::vector<TargetInst *> rewritten;
    for (auto *inst : block->GetInsts()) {
      auto *ai = static_cast<AArch64Inst *>(inst);
      if (ai->GetOpcode() != A64Opcode::RET) {
        rewritten.push_back(inst);
        continue;
      }
      for (size_t i = 0; i < callee_saved.size(); ++i) {
        auto [cls, id] = callee_saved[i];
        EmitStackAccess(&rewritten, A64Opcode::LDR_OFF, cs_reg(cls, id), static_cast<int>(cs_base + i * 8));
      }
      EmitStackAccess(&rewritten, A64Opcode::LDR_OFF, fp, fp_off);
      EmitStackAccess(&rewritten, A64Opcode::LDR_OFF, lr, lr_off);
      for (auto *add : MakeAddSp(frame)) {
        rewritten.push_back(add);
      }
      rewritten.push_back(ai);  // the bare `ret`
    }
    block->SetInsts(rewritten);
  }
}

}  // namespace backend::targetir::aarch64
