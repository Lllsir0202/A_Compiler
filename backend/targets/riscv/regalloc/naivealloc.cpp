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
// Value scratch registers, one per operand/result of a single instruction.
constexpr std::array<GPR, 3> VALUE_SCRATCH = {GPR::T0, GPR::T1, GPR::T2};
constexpr int IMM_MIN = -2048;
constexpr int IMM_MAX = 2047;

auto IsInImmRange(size_t offset) -> bool { return static_cast<long>(offset) <= IMM_MAX; }
}  // namespace

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
      size_t scratch = 0;
      for (auto *vreg : vops) {
        auto *preg = ctx_->PhysReg(TargetRegClass::General, TargetRegSize::QWord,
                                   static_cast<uint8_t>(VALUE_SCRATCH.at(scratch++)));
        auto reload = vreg->GetRegSize() == TargetRegSize::QWord ? RVOpcode::LD : RVOpcode::LW;
        EmitReload(&rewritten, reload, preg, SlotOf(vreg));
        inst->ReplaceOperand(vreg, preg);
      }

      auto *res = inst->GetResult();
      TargetVirtualReg *vres = res != nullptr && res->IsVirtualReg() ? static_cast<TargetVirtualReg *>(res) : nullptr;
      TargetOperand *res_preg = nullptr;
      if (vres != nullptr) {
        res_preg = ctx_->PhysReg(TargetRegClass::General, TargetRegSize::QWord,
                                 static_cast<uint8_t>(VALUE_SCRATCH.at(scratch++)));
        inst->ReplaceResult(res_preg);
      }
      rewritten.push_back(inst);
      if (vres != nullptr) {
        auto spill = vres->GetRegSize() == TargetRegSize::QWord ? RVOpcode::SD : RVOpcode::SW;
        EmitSpill(&rewritten, spill, res_preg, SlotOf(vres));
      }
    }
    block->SetInsts(rewritten);
  }
  return slot_.size();
}

}  // namespace backend::targetir::riscv
