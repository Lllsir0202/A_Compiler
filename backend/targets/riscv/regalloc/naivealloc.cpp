#include "backend/targets/riscv/regalloc/naivealloc.h"
#include <algorithm>
#include "backend/targets/riscv/regs/riscvreg.h"
#include "backend/targets/utils/operand/targetreg.h"
#include "backend/targets/utils/struct/targetblock.h"
#include "backend/targets/utils/struct/targetfunction.h"

namespace backend::targetir::riscv {

namespace {
constexpr auto SP = static_cast<uint8_t>(GPR::Sp);
constexpr auto T3 = static_cast<uint8_t>(GPR::T3);  // address scratch
// Value scratch registers, one per operand/result of a single instruction, kept
// separate per register class (integer t0-t2, float ft0-ft2).
constexpr std::array<GPR, 3> VALUE_SCRATCH = {GPR::T0, GPR::T1, GPR::T2};
constexpr std::array<FPR, 3> FLOAT_SCRATCH = {FPR::Ft0, FPR::Ft1, FPR::Ft2};
constexpr int IMM_MIN = -2048;
constexpr int IMM_MAX = 2047;

auto IsInImmRange(size_t offset) -> bool { return static_cast<long>(offset) <= IMM_MAX; }

auto ReloadOp(TargetVirtualReg *vreg) -> RVOpcode {
  if (vreg->GetRegClass() == TargetRegClass::Float) {
    return RVOpcode::FLW;
  }
  return vreg->GetRegSize() == TargetRegSize::QWord ? RVOpcode::LD : RVOpcode::LW;
}

auto SpillOp(TargetVirtualReg *vreg) -> RVOpcode {
  if (vreg->GetRegClass() == TargetRegClass::Float) {
    return RVOpcode::FSW;
  }
  return vreg->GetRegSize() == TargetRegSize::QWord ? RVOpcode::SD : RVOpcode::SW;
}
}  // namespace

auto NaiveRegAlloc::ScratchFor(TargetVirtualReg *vreg, size_t *gpr_scratch, size_t *fpr_scratch) -> TargetOperand * {
  if (vreg->GetRegClass() == TargetRegClass::Float) {
    return ctx_->PhysReg(TargetRegClass::Float, TargetRegSize::DWord,
                         static_cast<uint8_t>(FLOAT_SCRATCH.at((*fpr_scratch)++)));
  }
  return ctx_->PhysReg(TargetRegClass::General, TargetRegSize::QWord,
                       static_cast<uint8_t>(VALUE_SCRATCH.at((*gpr_scratch)++)));
}

auto NaiveRegAlloc::SlotOf(TargetVirtualReg *vreg) -> size_t {
  auto it = slot_.find(vreg);
  if (it != slot_.end()) {
    return it->second;
  }
  auto slot = slot_.size();
  slot_[vreg] = slot;
  return slot;
}

auto NaiveRegAlloc::EmitReload(std::vector<TargetInst *> *out, RVOpcode op, TargetOperand *dst, size_t slot) -> void {
  auto offset = alloca_base_ + slot * 8;
  auto *sp = ctx_->PhysReg(TargetRegClass::General, TargetRegSize::QWord, SP);
  if (IsInImmRange(offset)) {
    auto *ld = ctx_->CreateInst<RISCVInst>(op);
    ld->SetResult(dst);
    ld->AddOperand(sp);
    ld->AddOperand(ctx_->Imm(static_cast<int>(offset)));
    out->push_back(ld);
    return;
  }
  // Address does not fit an immediate: compute it in t3 first.
  auto *addr = ctx_->PhysReg(TargetRegClass::General, TargetRegSize::QWord, T3);
  auto *li = ctx_->CreateInst<RISCVInst>(RVOpcode::LI);
  li->SetResult(addr);
  li->AddOperand(ctx_->Imm(static_cast<int>(offset)));
  out->push_back(li);
  auto *add = ctx_->CreateInst<RISCVInst>(RVOpcode::ADD);
  add->SetResult(addr);
  add->AddOperand(sp);
  add->AddOperand(addr);
  out->push_back(add);
  auto *ld = ctx_->CreateInst<RISCVInst>(op);
  ld->SetResult(dst);
  ld->AddOperand(addr);
  ld->AddOperand(ctx_->Imm(0));
  out->push_back(ld);
}

auto NaiveRegAlloc::EmitSpill(std::vector<TargetInst *> *out, RVOpcode op, TargetOperand *src, size_t slot) -> void {
  auto offset = alloca_base_ + slot * 8;
  auto *sp = ctx_->PhysReg(TargetRegClass::General, TargetRegSize::QWord, SP);
  if (IsInImmRange(offset)) {
    auto *st = ctx_->CreateInst<RISCVInst>(op);
    st->AddOperand(src);
    st->AddOperand(sp);
    st->AddOperand(ctx_->Imm(static_cast<int>(offset)));
    out->push_back(st);
    return;
  }
  auto *addr = ctx_->PhysReg(TargetRegClass::General, TargetRegSize::QWord, T3);
  auto *li = ctx_->CreateInst<RISCVInst>(RVOpcode::LI);
  li->SetResult(addr);
  li->AddOperand(ctx_->Imm(static_cast<int>(offset)));
  out->push_back(li);
  auto *add = ctx_->CreateInst<RISCVInst>(RVOpcode::ADD);
  add->SetResult(addr);
  add->AddOperand(sp);
  add->AddOperand(addr);
  out->push_back(add);
  auto *st = ctx_->CreateInst<RISCVInst>(op);
  st->AddOperand(src);
  st->AddOperand(addr);
  st->AddOperand(ctx_->Imm(0));
  out->push_back(st);
}

auto NaiveRegAlloc::Run(TargetFunction *func, size_t alloca_base) -> size_t {
  slot_.clear();
  alloca_base_ = alloca_base;
  for (auto *block : func->GetBlocks()) {
    std::vector<TargetInst *> rewritten;
    for (auto *inst : block->GetInsts()) {
      // Distinct virtual operands, each reloaded once into its own scratch.
      std::vector<TargetVirtualReg *> vops;
      for (auto *op : inst->GetOperands()) {
        if (op->IsVirtualReg()) {
          auto *vreg = static_cast<TargetVirtualReg *>(op);
          if (std::find(vops.begin(), vops.end(), vreg) == vops.end()) {
            vops.push_back(vreg);
          }
        }
      }
      size_t gpr_scratch = 0;
      size_t fpr_scratch = 0;
      for (auto *vreg : vops) {
        auto *preg = ScratchFor(vreg, &gpr_scratch, &fpr_scratch);
        EmitReload(&rewritten, ReloadOp(vreg), preg, SlotOf(vreg));
        inst->ReplaceOperand(vreg, preg);
      }

      auto *res = inst->GetResult();
      TargetVirtualReg *vres = res != nullptr && res->IsVirtualReg() ? static_cast<TargetVirtualReg *>(res) : nullptr;
      TargetOperand *res_preg = nullptr;
      if (vres != nullptr) {
        res_preg = ScratchFor(vres, &gpr_scratch, &fpr_scratch);
        inst->ReplaceResult(res_preg);
      }
      rewritten.push_back(inst);
      if (vres != nullptr) {
        EmitSpill(&rewritten, SpillOp(vres), res_preg, SlotOf(vres));
      }
    }
    block->SetInsts(rewritten);
  }
  return slot_.size();
}

}  // namespace backend::targetir::riscv
