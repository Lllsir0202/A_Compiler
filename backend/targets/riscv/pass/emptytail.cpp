#include "backend/targets/riscv/pass/emptytail.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "backend/targets/riscv/inst/riscvinst.h"
#include "backend/targets/riscv/regs/riscvreginfo.h"
#include "backend/targets/utils/operand/targetliteral.h"
#include "backend/targets/utils/operand/targetreg.h"
#include "backend/targets/utils/operand/targetsymbol.h"
#include "backend/targets/utils/struct/targetblock.h"
#include "backend/targets/utils/struct/targetfunction.h"

namespace backend::targetir::riscv {

namespace {

auto SameRegister(TargetOperand *lhs, TargetOperand *rhs) -> bool {
  if (lhs == rhs) {
    return lhs != nullptr && (lhs->IsVirtualReg() || lhs->IsPhysicalReg());
  }
  if (lhs == nullptr || rhs == nullptr || !lhs->IsPhysicalReg() || !rhs->IsPhysicalReg()) {
    return false;
  }
  auto *left = static_cast<TargetPhysicalReg *>(lhs);
  auto *right = static_cast<TargetPhysicalReg *>(rhs);
  return left->GetRegClass() == right->GetRegClass() && left->GetID() == right->GetID();
}

auto LabelName(TargetOperand *operand) -> std::string {
  if (operand == nullptr || operand->GetOpType() != TargetOperandType::Label) {
    return {};
  }
  return static_cast<TargetLabel *>(operand)->GetName();
}

struct CFG {
  std::unordered_map<std::string, TargetBlock *> by_label_;
  std::unordered_map<TargetBlock *, std::vector<TargetBlock *>> successors_;
  std::unordered_map<TargetBlock *, std::vector<TargetBlock *>> predecessors_;
};

auto BuildCFG(TargetFunction *func) -> CFG {
  CFG cfg;
  for (auto *block : func->GetBlocks()) {
    cfg.by_label_[block->GetLabel()] = block;
  }
  for (auto *block : func->GetBlocks()) {
    for (auto *inst : block->GetInsts()) {
      auto opcode = static_cast<RISCVInst *>(inst)->GetOpcode();
      bool branch = opcode == RVOpcode::BEQ || opcode == RVOpcode::BNE || opcode == RVOpcode::BLT ||
                    opcode == RVOpcode::BGE || opcode == RVOpcode::BLTU || opcode == RVOpcode::BGEU ||
                    opcode == RVOpcode::J;
      if (!branch || inst->GetOperands().empty()) {
        continue;
      }
      auto target = cfg.by_label_.find(LabelName(inst->GetOperands().back()));
      if (target == cfg.by_label_.end()) {
        continue;
      }
      auto &successors = cfg.successors_[block];
      if (std::find(successors.begin(), successors.end(), target->second) == successors.end()) {
        successors.push_back(target->second);
        cfg.predecessors_[target->second].push_back(block);
      }
    }
  }
  return cfg;
}

auto JumpTarget(TargetBlock *block, const CFG &cfg) -> TargetBlock * {
  const auto &insts = block->GetInsts();
  if (insts.empty()) {
    return nullptr;
  }
  auto *jump = static_cast<RISCVInst *>(insts.back());
  if (jump->GetOpcode() != RVOpcode::J || jump->GetOperands().size() != 1) {
    return nullptr;
  }
  auto found = cfg.by_label_.find(LabelName(jump->GetOperands()[0]));
  return found == cfg.by_label_.end() ? nullptr : found->second;
}

auto HasBranchAndJump(TargetBlock *block, RVOpcode opcode) -> bool {
  const auto &insts = block->GetInsts();
  return insts.size() >= 2 && static_cast<RISCVInst *>(insts[insts.size() - 2])->GetOpcode() == opcode &&
         static_cast<RISCVInst *>(insts.back())->GetOpcode() == RVOpcode::J;
}

auto RegisterLiveIn(TargetFunction *func, const CFG &cfg, TargetBlock *query, TargetOperand *reg) -> bool {
  RISCVRegisterInfo reg_info;
  std::unordered_map<TargetBlock *, bool> use;
  std::unordered_map<TargetBlock *, bool> def;
  std::unordered_map<TargetBlock *, bool> live_in;
  std::unordered_map<TargetBlock *, bool> live_out;

  for (auto *block : func->GetBlocks()) {
    bool killed = false;
    for (auto *inst : block->GetInsts()) {
      for (auto *operand : inst->GetOperands()) {
        if (!killed && SameRegister(operand, reg)) {
          use[block] = true;
        }
      }
      if (SameRegister(inst->GetResult(), reg)) {
        killed = true;
        def[block] = true;
      }
      if (static_cast<RISCVInst *>(inst)->GetOpcode() == RVOpcode::CALL && reg->IsPhysicalReg()) {
        auto *physical = static_cast<TargetPhysicalReg *>(reg);
        if (reg_info.IsCallerSaved(physical->GetRegClass(), physical->GetID())) {
          killed = true;
          def[block] = true;
        }
      }
    }
  }

  bool changed = true;
  while (changed) {
    changed = false;
    const auto &blocks = func->GetBlocks();
    for (auto it = blocks.rbegin(); it != blocks.rend(); ++it) {
      auto *block = *it;
      bool out = false;
      auto successors = cfg.successors_.find(block);
      if (successors != cfg.successors_.end()) {
        for (auto *successor : successors->second) {
          out |= live_in[successor];
        }
      }
      bool in = use[block] || (out && !def[block]);
      if (out != live_out[block] || in != live_in[block]) {
        live_out[block] = out;
        live_in[block] = in;
        changed = true;
      }
    }
  }
  return live_in[query];
}

auto TryBypass(TargetFunction *func, const CFG &cfg, TargetBlock *header) -> bool {
  if (!HasBranchAndJump(header, RVOpcode::BLT)) {
    return false;
  }
  const auto &header_insts = header->GetInsts();
  auto *tail_branch = header_insts[header_insts.size() - 2];
  const auto &tail_ops = tail_branch->GetOperands();
  if (tail_ops.size() != 3 || !tail_ops[0]->IsPhysicalReg() || !tail_ops[1]->IsPhysicalReg()) {
    return false;
  }
  auto *iv = tail_ops[1];
  auto skipped = cfg.by_label_.find(LabelName(tail_ops[2]));
  if (skipped == cfg.by_label_.end()) {
    return false;
  }
  auto *advance = skipped->second;
  auto advance_preds = cfg.predecessors_.find(advance);
  if (advance_preds == cfg.predecessors_.end() || advance_preds->second.size() != 1 ||
      advance_preds->second[0] != header) {
    return false;
  }

  const auto &advance_insts = advance->GetInsts();
  if (advance_insts.size() != 2 || static_cast<RISCVInst *>(advance_insts[0])->GetOpcode() != RVOpcode::ADDIW ||
      static_cast<RISCVInst *>(advance_insts[1])->GetOpcode() != RVOpcode::J) {
    return false;
  }
  const auto &increment_ops = advance_insts[0]->GetOperands();
  if (increment_ops.size() != 2 || !SameRegister(advance_insts[0]->GetResult(), iv) ||
      !SameRegister(increment_ops[0], iv) || increment_ops[1]->GetOpType() != TargetOperandType::Immediate ||
      static_cast<TargetIntLiteral *>(increment_ops[1])->GetValue() != 1) {
    return false;
  }

  auto *latch = JumpTarget(advance, cfg);
  if (latch == nullptr || !HasBranchAndJump(latch, RVOpcode::BLT)) {
    return false;
  }
  const auto &latch_insts = latch->GetInsts();
  auto *loop_branch = latch_insts[latch_insts.size() - 2];
  const auto &loop_ops = loop_branch->GetOperands();
  if (latch_insts.size() != 2 || loop_ops.size() != 3 || !SameRegister(loop_ops[0], iv)) {
    return false;
  }
  auto back = cfg.by_label_.find(LabelName(loop_ops[2]));
  auto *exit = JumpTarget(latch, cfg);
  if (back == cfg.by_label_.end() || exit == nullptr) {
    return false;
  }
  auto *backedge = back->second;
  if (backedge->GetInsts().size() != 1 || JumpTarget(backedge, cfg) != header) {
    return false;
  }

  auto header_preds = cfg.predecessors_.find(header);
  if (header_preds == cfg.predecessors_.end() || header_preds->second.size() != 2 ||
      std::find(header_preds->second.begin(), header_preds->second.end(), backedge) == header_preds->second.end()) {
    return false;
  }
  auto *entry = header_preds->second[0] == backedge ? header_preds->second[1] : header_preds->second[0];
  const auto &entry_insts = entry->GetInsts();
  if (entry_insts.size() != 2 || static_cast<RISCVInst *>(entry_insts[0])->GetOpcode() != RVOpcode::MV ||
      JumpTarget(entry, cfg) != header || !SameRegister(entry_insts[0]->GetResult(), iv) ||
      entry_insts[0]->GetOperands().size() != 1) {
    return false;
  }
  auto *initial = entry_insts[0]->GetOperands()[0];
  auto entry_preds = cfg.predecessors_.find(entry);
  if (entry_preds == cfg.predecessors_.end() || entry_preds->second.size() != 1) {
    return false;
  }
  auto *guard = entry_preds->second[0];
  if (!HasBranchAndJump(guard, RVOpcode::BLT) || JumpTarget(guard, cfg) != exit) {
    return false;
  }
  const auto &guard_insts = guard->GetInsts();
  const auto &guard_ops = guard_insts[guard_insts.size() - 2]->GetOperands();
  if (guard_ops.size() != 3 || !SameRegister(guard_ops[0], initial) || !SameRegister(guard_ops[1], loop_ops[1]) ||
      LabelName(guard_ops[2]) != entry->GetLabel()) {
    return false;
  }

  // Bypassing leaves IV at its current value instead of the loop bound.  This
  // is observable only if IV is live on the exit edge.
  if (RegisterLiveIn(func, cfg, exit, iv)) {
    return false;
  }

  tail_branch->SetOperands({tail_ops[0], tail_ops[1], latch_insts.back()->GetOperands()[0]});
  return true;
}

}  // namespace

auto RISCVEmptyTail::RunOnFunction(TargetFunction *func) -> bool {
  auto cfg = BuildCFG(func);
  bool changed = false;
  for (auto *block : func->GetBlocks()) {
    changed |= TryBypass(func, cfg, block);
  }
  return changed;
}

}  // namespace backend::targetir::riscv
