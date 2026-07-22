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

auto RISCVFrame::EmitAccess(std::vector<TargetInst *> *out, RVOpcode op, TargetOperand *reg, TargetOperand *base,
                            int offset) -> void {
  bool is_load = op == RVOpcode::LD || op == RVOpcode::LW || op == RVOpcode::FLW;
  auto *inst = ctx_->CreateInst<RISCVInst>(op);
  if (is_load) {  // load: rd, imm(base)
    inst->SetResult(reg);
    inst->AddOperand(base);
    inst->AddOperand(ctx_->Imm(offset));
  } else {  // store: reg, imm(base)
    inst->AddOperand(reg);
    inst->AddOperand(base);
    inst->AddOperand(ctx_->Imm(offset));
  }
  out->push_back(inst);
}

auto RISCVFrame::EmitStackAccess(std::vector<TargetInst *> *out, RVOpcode op, TargetOperand *reg, int offset) -> void {
  auto *sp = ctx_->PhysReg(TargetRegClass::General, TargetRegSize::QWord, SP);
  if (offset >= -2048 && offset <= IMM_MAX) {
    EmitAccess(out, op, reg, sp, offset);
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
  EmitAccess(out, op, reg, addr, 0);
}

// Save (or restore) the contiguous run of callee-saved registers, whose slots
// sit at `base_offset + i*8` off sp. When the whole run is immediate-addressable
// from sp, each register is a single sp-relative access. Otherwise a scratch
// base (t3 = sp + base_offset) is materialized once and every slot reuses it
// with a small offset, turning 3 instructions per slot into 2 + N for the run.
// The store/load opcode is chosen per slot from its register class, so int (SD)
// and float (FSW) saves can share the same base.
auto RISCVFrame::EmitCalleeSavedRun(std::vector<TargetInst *> *out,
                                    const std::vector<std::pair<TargetRegClass, uint8_t>> &regs,
                                    const std::function<TargetOperand *(TargetRegClass, uint8_t)> &make_reg,
                                    int base_offset, bool restore) -> void {
  if (regs.empty()) {
    return;
  }
  auto op_for = [&](TargetRegClass cls) {
    if (cls == TargetRegClass::Float) {
      return restore ? RVOpcode::FLW : RVOpcode::FSW;
    }
    return restore ? RVOpcode::LD : RVOpcode::SD;
  };
  auto *sp = ctx_->PhysReg(TargetRegClass::General, TargetRegSize::QWord, SP);
  int last_offset = base_offset + static_cast<int>((regs.size() - 1) * 8);
  // Whole run addressable directly from sp: keep the simple per-slot form.
  if (base_offset >= -2048 && last_offset <= IMM_MAX) {
    for (size_t i = 0; i < regs.size(); ++i) {
      auto [cls, id] = regs[i];
      EmitAccess(out, op_for(cls), make_reg(cls, id), sp, base_offset + static_cast<int>(i * 8));
    }
    return;
  }
  // Materialize a shared base once (t3 = sp + base_offset); t3 is reserved
  // scratch and never a callee-saved register, so reusing it here is safe.
  auto *base = ctx_->PhysReg(TargetRegClass::General, TargetRegSize::QWord, T3);
  auto *li = ctx_->CreateInst<RISCVInst>(RVOpcode::LI);
  li->SetResult(base);
  li->AddOperand(ctx_->Imm(base_offset));
  out->push_back(li);
  auto *add = ctx_->CreateInst<RISCVInst>(RVOpcode::ADD);
  add->SetResult(base);
  add->AddOperand(sp);
  add->AddOperand(base);
  out->push_back(add);
  for (size_t i = 0; i < regs.size(); ++i) {
    auto [cls, id] = regs[i];
    EmitAccess(out, op_for(cls), make_reg(cls, id), base, static_cast<int>(i * 8));
  }
}

auto RISCVFrame::Finalize(TargetFunction *func, size_t locals_bytes, size_t spill_slots,
                          const std::vector<std::pair<TargetRegClass, uint8_t>> &callee_saved) -> void {
  const auto &blocks = func->GetBlocks();
  if (blocks.empty()) {
    return;
  }
  // ra (x1) and fp (s0) are non-allocatable, so the body only mentions them for
  // what the frame layer itself needs: `call` is the sole writer of ra, and fp is
  // read only to reach incoming stack arguments. A leaf that touches neither skips
  // the matching save/restore (and the fp set-up), collapsing a small leaf's whole
  // prologue/epilogue to nothing.
  bool has_call = false;
  bool uses_fp = false;
  for (auto *block : blocks) {
    for (auto *inst : block->GetInsts()) {
      if (static_cast<RISCVInst *>(inst)->GetOpcode() == RVOpcode::CALL) {
        has_call = true;
      }
      auto is_fp = [](TargetOperand *op) {
        return op != nullptr && op->IsPhysicalReg() && static_cast<TargetPhysicalReg *>(op)->GetID() == FP;
      };
      if (is_fp(inst->GetResult())) {
        uses_fp = true;
      }
      for (auto *op : inst->GetOperands()) {
        if (is_fp(op)) {
          uses_fp = true;
        }
      }
    }
  }
  bool save_ra = has_call;  // `call` is the only writer of ra
  bool save_fp = uses_fp;   // set fp (and thus preserve it) only if the body reads it

  // Layout (low -> high): outgoing-args + alloca (locals) | spill slots |
  // callee-saved register saves | saved fp | saved ra. 16-byte aligned.
  size_t cs_base = locals_bytes + spill_slots * 8;
  size_t top = cs_base + callee_saved.size() * 8;
  int fp_off = static_cast<int>(top);
  int ra_off = static_cast<int>(top + (save_fp ? 8U : 0U));
  size_t saved = (save_fp ? 8U : 0U) + (save_ra ? 8U : 0U);
  auto frame = static_cast<int>(AlignUp16(top + saved));

  auto *ra = ctx_->PhysReg(TargetRegClass::General, TargetRegSize::QWord, RA);
  auto *fp = ctx_->PhysReg(TargetRegClass::General, TargetRegSize::QWord, FP);

  auto cs_reg = [&](TargetRegClass cls, uint8_t id) {
    return ctx_->PhysReg(cls, cls == TargetRegClass::Float ? TargetRegSize::DWord : TargetRegSize::QWord, id);
  };

  // Prologue: sp -= frame; save ra & caller's fp; fp = incoming sp (= sp + frame),
  // which addresses incoming stack arguments independent of the frame size; then
  // preserve the callee-saved registers the allocator used.
  std::vector<TargetInst *> prologue;
  if (frame != 0) {
    prologue = MakeAddSp(-frame);
  }
  if (save_ra) {
    EmitStackAccess(&prologue, RVOpcode::SD, ra, ra_off);
  }
  if (save_fp) {
    EmitStackAccess(&prologue, RVOpcode::SD, fp, fp_off);
    for (auto *add : MakeSetFp(frame)) {
      prologue.push_back(add);
    }
  }
  // The callee-saved slots are contiguous, so save them as one run: a large
  // frame then materializes the slot base once instead of once per register.
  EmitCalleeSavedRun(&prologue, callee_saved, cs_reg, static_cast<int>(cs_base), /*restore=*/false);

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
      EmitCalleeSavedRun(&rewritten, callee_saved, cs_reg, static_cast<int>(cs_base), /*restore=*/true);
      if (save_ra) {
        EmitStackAccess(&rewritten, RVOpcode::LD, ra, ra_off);
      }
      if (save_fp) {
        EmitStackAccess(&rewritten, RVOpcode::LD, fp, fp_off);
      }
      if (frame != 0) {
        for (auto *add : MakeAddSp(frame)) {
          rewritten.push_back(add);
        }
      }
      rewritten.push_back(ri);  // the bare `ret`
    }
    block->SetInsts(rewritten);
  }
}

}  // namespace backend::targetir::riscv
