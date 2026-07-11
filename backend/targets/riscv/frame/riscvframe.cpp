#include "backend/targets/riscv/frame/riscvframe.h"
#include "backend/targets/riscv/regs/riscvreg.h"
#include "backend/targets/utils/operand/targetreg.h"
#include "backend/targets/utils/struct/targetblock.h"
#include "backend/targets/utils/struct/targetfunction.h"

namespace backend::targetir::riscv {

namespace {
constexpr auto SP = static_cast<uint8_t>(GPR::Sp);
constexpr auto RA = static_cast<uint8_t>(GPR::Ra);
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

auto RISCVFrame::Finalize(TargetFunction *func, size_t alloca_bytes, size_t spill_slots) -> void {
  const auto &blocks = func->GetBlocks();
  if (blocks.empty()) {
    return;
  }
  // alloca area + spill area + saved ra, 16-byte aligned.
  auto frame = static_cast<int>(AlignUp16(alloca_bytes + spill_slots * 8 + 8));
  int ra_off = frame - 8;

  auto *sp = ctx_->PhysReg(TargetRegClass::General, TargetRegSize::QWord, SP);
  auto *ra = ctx_->PhysReg(TargetRegClass::General, TargetRegSize::QWord, RA);

  // Prologue: sp -= frame; sd ra, ra_off(sp).
  std::vector<TargetInst *> prologue = MakeAddSp(-frame);
  auto *save_ra = ctx_->CreateInst<RISCVInst>(RVOpcode::SD);
  save_ra->AddOperand(ra);
  save_ra->AddOperand(sp);
  save_ra->AddOperand(ctx_->Imm(ra_off));
  prologue.push_back(save_ra);

  auto *entry = blocks.front();
  std::vector<TargetInst *> new_entry = prologue;
  for (auto *inst : entry->GetInsts()) {
    new_entry.push_back(inst);
  }
  entry->SetInsts(new_entry);

  // Epilogue: expand every `ret` into ld ra, ra_off(sp); sp += frame; ret.
  for (auto *block : blocks) {
    std::vector<TargetInst *> rewritten;
    for (auto *inst : block->GetInsts()) {
      auto *ri = static_cast<RISCVInst *>(inst);
      if (ri->GetOpcode() != RVOpcode::RET) {
        rewritten.push_back(inst);
        continue;
      }
      auto *load_ra = ctx_->CreateInst<RISCVInst>(RVOpcode::LD);
      load_ra->SetResult(ra);
      load_ra->AddOperand(sp);
      load_ra->AddOperand(ctx_->Imm(ra_off));
      rewritten.push_back(load_ra);
      for (auto *add : MakeAddSp(frame)) {
        rewritten.push_back(add);
      }
      rewritten.push_back(ri);  // the bare `ret`
    }
    block->SetInsts(rewritten);
  }
}

}  // namespace backend::targetir::riscv
