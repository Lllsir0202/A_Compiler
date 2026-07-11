#include "backend/targets/riscv/frame/riscvframe.h"
#include "backend/targets/riscv/regs/riscvreg.h"
#include "backend/targets/utils/operand/targetreg.h"
#include "backend/targets/utils/struct/targetblock.h"
#include "backend/targets/utils/struct/targetfunction.h"

namespace backend::targetir::riscv {

namespace {
constexpr auto SP = static_cast<uint8_t>(GPR::Sp);
constexpr auto RA = static_cast<uint8_t>(GPR::Ra);
constexpr auto FP = static_cast<uint8_t>(GPR::Fp);
constexpr auto T3 = static_cast<uint8_t>(GPR::T3);
constexpr int IMM_MAX = 2047;

auto AlignUp16(size_t n) -> size_t { return (n + 15U) & ~static_cast<size_t>(15U); }
}  // namespace

auto RISCVFrame::MakeAddSp(int delta) -> std::vector<TargetInst *> {
  std::vector<TargetInst *> seq;
  auto *sp = ctx_->PhysReg(TargetRegClass::General, TargetRegSize::QWord, SP);
  if (delta >= -2048 && delta <= IMM_MAX) {
    auto *addi = ctx_->CreateInst<RISCVInst>(RVOpcode::ADDI);
    addi->SetResult(sp);
    addi->AddOperand(sp);
    addi->AddOperand(ctx_->Imm(delta));
    seq.push_back(addi);
    return seq;
  }
  // Out of immediate range: materialize the magnitude and add/sub via t3.
  auto *tmp = ctx_->PhysReg(TargetRegClass::General, TargetRegSize::QWord, T3);
  auto *li = ctx_->CreateInst<RISCVInst>(RVOpcode::LI);
  li->SetResult(tmp);
  li->AddOperand(ctx_->Imm(delta < 0 ? -delta : delta));
  seq.push_back(li);
  auto *op = ctx_->CreateInst<RISCVInst>(delta < 0 ? RVOpcode::SUB : RVOpcode::ADD);
  op->SetResult(sp);
  op->AddOperand(sp);
  op->AddOperand(tmp);
  seq.push_back(op);
  return seq;
}

auto RISCVFrame::MakeSetFp(int frame) -> std::vector<TargetInst *> {
  // fp = sp + frame (the incoming stack pointer), immediate-legalized.
  std::vector<TargetInst *> seq;
  auto *sp = ctx_->PhysReg(TargetRegClass::General, TargetRegSize::QWord, SP);
  auto *fp = ctx_->PhysReg(TargetRegClass::General, TargetRegSize::QWord, FP);
  if (frame <= IMM_MAX) {
    auto *addi = ctx_->CreateInst<RISCVInst>(RVOpcode::ADDI);
    addi->SetResult(fp);
    addi->AddOperand(sp);
    addi->AddOperand(ctx_->Imm(frame));
    seq.push_back(addi);
    return seq;
  }
  auto *tmp = ctx_->PhysReg(TargetRegClass::General, TargetRegSize::QWord, T3);
  auto *li = ctx_->CreateInst<RISCVInst>(RVOpcode::LI);
  li->SetResult(tmp);
  li->AddOperand(ctx_->Imm(frame));
  seq.push_back(li);
  auto *add = ctx_->CreateInst<RISCVInst>(RVOpcode::ADD);
  add->SetResult(fp);
  add->AddOperand(sp);
  add->AddOperand(tmp);
  seq.push_back(add);
  return seq;
}

auto RISCVFrame::EmitStackAccess(std::vector<TargetInst *> *out, RVOpcode op, TargetOperand *reg, int offset) -> void {
  auto *sp = ctx_->PhysReg(TargetRegClass::General, TargetRegSize::QWord, SP);
  bool is_load = op == RVOpcode::LD || op == RVOpcode::LW || op == RVOpcode::FLW;
  auto build = [&](TargetOperand *base, int imm) {
    auto *inst = ctx_->CreateInst<RISCVInst>(op);
    if (is_load) {  // load: rd, imm(base)
      inst->SetResult(reg);
      inst->AddOperand(base);
      inst->AddOperand(ctx_->Imm(imm));
    } else {  // store: reg, imm(base)
      inst->AddOperand(reg);
      inst->AddOperand(base);
      inst->AddOperand(ctx_->Imm(imm));
    }
    out->push_back(inst);
  };
  if (offset >= -2048 && offset <= IMM_MAX) {
    build(sp, offset);
    return;
  }
  auto *addr = ctx_->PhysReg(TargetRegClass::General, TargetRegSize::QWord, T3);
  auto *li = ctx_->CreateInst<RISCVInst>(RVOpcode::LI);
  li->SetResult(addr);
  li->AddOperand(ctx_->Imm(offset));
  out->push_back(li);
  auto *add = ctx_->CreateInst<RISCVInst>(RVOpcode::ADD);
  add->SetResult(addr);
  add->AddOperand(sp);
  add->AddOperand(addr);
  out->push_back(add);
  build(addr, 0);
}

auto RISCVFrame::Finalize(TargetFunction *func, size_t locals_bytes, size_t spill_slots,
                          const std::vector<std::pair<TargetRegClass, uint8_t>> &callee_saved) -> void {
  const auto &blocks = func->GetBlocks();
  if (blocks.empty()) {
    return;
  }
  // Layout (low -> high): outgoing-args + alloca (locals) | spill slots |
  // callee-saved register saves | saved fp | saved ra. 16-byte aligned.
  size_t cs_base = locals_bytes + spill_slots * 8;
  auto frame = static_cast<int>(AlignUp16(cs_base + callee_saved.size() * 8 + 16));
  int ra_off = frame - 8;
  int fp_off = frame - 16;

  auto *ra = ctx_->PhysReg(TargetRegClass::General, TargetRegSize::QWord, RA);
  auto *fp = ctx_->PhysReg(TargetRegClass::General, TargetRegSize::QWord, FP);

  // Store (prologue) / load (epilogue) opcode for a callee-saved register.
  auto save_op = [](TargetRegClass cls) { return cls == TargetRegClass::Float ? RVOpcode::FSW : RVOpcode::SD; };
  auto restore_op = [](TargetRegClass cls) { return cls == TargetRegClass::Float ? RVOpcode::FLW : RVOpcode::LD; };
  auto cs_reg = [&](TargetRegClass cls, uint8_t id) {
    return ctx_->PhysReg(cls, cls == TargetRegClass::Float ? TargetRegSize::DWord : TargetRegSize::QWord, id);
  };

  // Prologue: sp -= frame; save ra & caller's fp; fp = incoming sp (= sp + frame),
  // which addresses incoming stack arguments independent of the frame size; then
  // preserve the callee-saved registers the allocator used.
  std::vector<TargetInst *> prologue = MakeAddSp(-frame);
  EmitStackAccess(&prologue, RVOpcode::SD, ra, ra_off);
  EmitStackAccess(&prologue, RVOpcode::SD, fp, fp_off);
  for (auto *add : MakeSetFp(frame)) {
    prologue.push_back(add);
  }
  for (size_t i = 0; i < callee_saved.size(); ++i) {
    auto [cls, id] = callee_saved[i];
    EmitStackAccess(&prologue, save_op(cls), cs_reg(cls, id), static_cast<int>(cs_base + i * 8));
  }

  auto *entry = blocks.front();
  std::vector<TargetInst *> new_entry = prologue;
  for (auto *inst : entry->GetInsts()) {
    new_entry.push_back(inst);
  }
  entry->SetInsts(new_entry);

  // Epilogue: restore callee-saved regs & ra & fp, release the frame, return.
  for (auto *block : blocks) {
    std::vector<TargetInst *> rewritten;
    for (auto *inst : block->GetInsts()) {
      auto *ri = static_cast<RISCVInst *>(inst);
      if (ri->GetOpcode() != RVOpcode::RET) {
        rewritten.push_back(inst);
        continue;
      }
      for (size_t i = 0; i < callee_saved.size(); ++i) {
        auto [cls, id] = callee_saved[i];
        EmitStackAccess(&rewritten, restore_op(cls), cs_reg(cls, id), static_cast<int>(cs_base + i * 8));
      }
      EmitStackAccess(&rewritten, RVOpcode::LD, ra, ra_off);
      EmitStackAccess(&rewritten, RVOpcode::LD, fp, fp_off);
      for (auto *add : MakeAddSp(frame)) {
        rewritten.push_back(add);
      }
      rewritten.push_back(ri);  // the bare `ret`
    }
    block->SetInsts(rewritten);
  }
}

}  // namespace backend::targetir::riscv
