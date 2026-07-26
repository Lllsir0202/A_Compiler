#include "backend/targets/riscv/pass/machinephielim.h"

#include <algorithm>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "backend/targets/riscv/inst/riscvinst.h"
#include "backend/targets/riscv/pass/machineanalysis.h"
#include "backend/targets/riscv/regs/riscvreg.h"
#include "backend/targets/utils/context/targetcontext.h"
#include "backend/targets/utils/operand/targetreg.h"
#include "backend/targets/utils/operand/targetsymbol.h"
#include "backend/targets/utils/struct/targetblock.h"
#include "backend/targets/utils/struct/targetfunction.h"

namespace backend::targetir::riscv {

namespace {
constexpr auto T3 = static_cast<uint8_t>(GPR::T3);

auto RegClass(TargetOperand *operand) -> TargetRegClass {
  if (operand->IsVirtualReg()) {
    return static_cast<TargetVirtualReg *>(operand)->GetRegClass();
  }
  return static_cast<TargetPhysicalReg *>(operand)->GetRegClass();
}

auto RegSize(TargetOperand *operand) -> TargetRegSize {
  if (operand->IsVirtualReg()) {
    return static_cast<TargetVirtualReg *>(operand)->GetRegSize();
  }
  return static_cast<TargetPhysicalReg *>(operand)->GetRegSize();
}

auto SameRegister(TargetOperand *lhs, TargetOperand *rhs) -> bool {
  if (lhs == rhs) {
    return true;
  }
  if (!lhs->IsPhysicalReg() || !rhs->IsPhysicalReg()) {
    return false;
  }
  auto *lhs_reg = static_cast<TargetPhysicalReg *>(lhs);
  auto *rhs_reg = static_cast<TargetPhysicalReg *>(rhs);
  return lhs_reg->GetRegClass() == rhs_reg->GetRegClass() && lhs_reg->GetID() == rhs_reg->GetID();
}

auto CopyOpcode(TargetOperand *destination, TargetOperand *source) -> RVOpcode {
  auto destination_class = RegClass(destination);
  auto source_class = RegClass(source);
  if (destination_class == TargetRegClass::Float) {
    return source_class == TargetRegClass::Float ? RVOpcode::FMV_S : RVOpcode::FMV_W_X;
  }
  return source_class == TargetRegClass::Float ? RVOpcode::FMV_X_W : RVOpcode::MV;
}

auto BranchTargets(TargetBlock *block) -> std::unordered_set<std::string> {
  std::unordered_set<std::string> targets;
  for (auto *inst : block->GetInsts()) {
    const auto &info = GetRVOpcodeInfo(static_cast<RISCVInst *>(inst)->GetOpcode());
    if (!info.HasFlag(RVInstFlag::Branch)) {
      continue;
    }
    for (auto *operand : inst->GetOperands()) {
      if (operand->GetOpType() == TargetOperandType::Label) {
        targets.insert(static_cast<TargetLabel *>(operand)->GetName());
      }
    }
  }
  return targets;
}

}  // namespace

auto MachinePhiElimination::InsertBeforeTerminators(TargetBlock *block, const std::vector<TargetInst *> &code) -> void {
  if (code.empty()) {
    return;
  }
  std::vector<TargetInst *> rewritten;
  bool inserted = false;
  for (auto *inst : block->GetInsts()) {
    const auto &info = GetRVOpcodeInfo(static_cast<RISCVInst *>(inst)->GetOpcode());
    if (!inserted && info.HasFlag(RVInstFlag::Terminator)) {
      rewritten.insert(rewritten.end(), code.begin(), code.end());
      inserted = true;
    }
    rewritten.push_back(inst);
  }
  if (!inserted) {
    rewritten.insert(rewritten.end(), code.begin(), code.end());
  }
  block->SetInsts(std::move(rewritten));
}

auto MachinePhiElimination::EmitParallelCopies(TargetBlock *block, std::vector<Copy> copies) -> void {
  std::vector<Copy> work;
  for (const auto &copy : copies) {
    if (!SameRegister(copy.first, copy.second)) {
      work.push_back(copy);
    }
  }
  std::vector<TargetInst *> emitted;
  while (!work.empty()) {
    auto ready = work.end();
    for (auto it = work.begin(); it != work.end(); ++it) {
      bool destination_is_source = false;
      for (const auto &other : work) {
        if (SameRegister(other.second, it->first)) {
          destination_is_source = true;
          break;
        }
      }
      if (!destination_is_source) {
        ready = it;
        break;
      }
    }

    if (ready != work.end()) {
      auto *move = ctx_->CreateInst<RISCVInst>(CopyOpcode(ready->first, ready->second));
      move->SetResult(ready->first);
      move->AddOperand(ready->second);
      emitted.push_back(move);
      work.erase(ready);
      continue;
    }

    // A parallel-copy cycle. Preserve one old destination and redirect every
    // reader of that old value to the temporary.
    auto *destination = work.front().first;
    TargetOperand *temporary = nullptr;
    if (destination->IsVirtualReg()) {
      temporary = ctx_->NewVirtualReg(RegClass(destination), RegSize(destination));
    } else {
      temporary = ctx_->PhysReg(TargetRegClass::General, TargetRegSize::QWord, T3);
    }
    auto *save = ctx_->CreateInst<RISCVInst>(CopyOpcode(temporary, destination));
    save->SetResult(temporary);
    save->AddOperand(destination);
    emitted.push_back(save);
    for (auto &copy : work) {
      if (SameRegister(copy.second, destination)) {
        copy.second = temporary;
      }
    }
  }
  InsertBeforeTerminators(block, emitted);
}

auto MachinePhiElimination::PrepareEdges(TargetFunction *func) -> bool {
  MachineCFG cfg(func);
  std::unordered_map<std::string, TargetBlock *> by_label;
  size_t next_block_id = 0;
  for (auto *block : func->GetBlocks()) {
    by_label[block->GetLabel()] = block;
    next_block_id = std::max(next_block_id, block->GetID() + 1);
  }

  using Edge = std::pair<TargetBlock *, TargetBlock *>;
  std::map<Edge, std::vector<TargetInst *>> edges;
  for (const auto &[labels, code] : func->GetAllEdgeCode()) {
    auto predecessor = by_label.find(labels.first);
    auto successor = by_label.find(labels.second);
    if (predecessor != by_label.end() && successor != by_label.end()) {
      edges[{predecessor->second, successor->second}] = code;
    }
  }
  for (auto *successor : func->GetBlocks()) {
    for (auto *instruction : successor->GetInsts()) {
      if (static_cast<RISCVInst *>(instruction)->GetOpcode() != RVOpcode::PHI) {
        break;
      }
      const auto &operands = instruction->GetOperands();
      for (size_t index = 0; index + 1 < operands.size(); index += 2) {
        auto *label = static_cast<TargetLabel *>(operands[index + 1]);
        auto predecessor = by_label.find(label->GetName());
        if (predecessor != by_label.end()) {
          edges.try_emplace({predecessor->second, successor});
        }
      }
    }
  }

  bool changed = false;
  std::vector<TargetBlock *> blocks = func->GetBlocks();
  for (const auto &[edge, code] : edges) {
    auto *predecessor = edge.first;
    auto *successor = edge.second;
    if (cfg.Successors(predecessor).size() <= 1) {
      if (!code.empty()) {
        InsertBeforeTerminators(predecessor, code);
        changed = true;
      }
      continue;
    }

    auto *split = ctx_->NewBlock(next_block_id++);
    split->SetLabel(".L_" + func->GetName() + "_phi_" + std::to_string(split->GetID()));
    InsertBeforeTerminators(split, code);
    auto *jump = ctx_->CreateInst<RISCVInst>(RVOpcode::J);
    jump->AddOperand(ctx_->Label(successor->GetLabel()));
    split->AddInst(jump);

    bool redirected = false;
    for (auto *instruction : predecessor->GetInsts()) {
      std::vector<TargetOperand *> operands = instruction->GetOperands();
      bool rewritten = false;
      for (auto *&operand : operands) {
        if (operand->GetOpType() == TargetOperandType::Label &&
            static_cast<TargetLabel *>(operand)->GetName() == successor->GetLabel()) {
          operand = ctx_->Label(split->GetLabel());
          rewritten = true;
        }
      }
      if (rewritten) {
        instruction->SetOperands(std::move(operands));
        redirected = true;
      }
    }
    if (!redirected) {
      // The selected CFG may represent the second edge of a conditional as an
      // implicit fallthrough. Once its dedicated block is appended elsewhere
      // in layout order, make that edge explicit.
      auto *jump_to_split = ctx_->CreateInst<RISCVInst>(RVOpcode::J);
      jump_to_split->AddOperand(ctx_->Label(split->GetLabel()));
      predecessor->AddInst(jump_to_split);
    }
    for (auto *instruction : successor->GetInsts()) {
      if (static_cast<RISCVInst *>(instruction)->GetOpcode() != RVOpcode::PHI) {
        break;
      }
      std::vector<TargetOperand *> operands = instruction->GetOperands();
      for (size_t index = 1; index < operands.size(); index += 2) {
        if (static_cast<TargetLabel *>(operands[index])->GetName() == predecessor->GetLabel()) {
          operands[index] = ctx_->Label(split->GetLabel());
        }
      }
      instruction->SetOperands(std::move(operands));
    }
    blocks.push_back(split);
    changed = true;
  }
  func->SetBlocks(std::move(blocks));
  func->ClearEdgeCode();
  return changed;
}

auto MachinePhiElimination::RunOnFunction(TargetFunction *func) -> bool {
  PrepareEdges(func);
  std::unordered_map<std::string, TargetBlock *> by_label;
  size_t next_block_id = 0;
  for (auto *block : func->GetBlocks()) {
    by_label[block->GetLabel()] = block;
    next_block_id = std::max(next_block_id, block->GetID() + 1);
  }

  using Edge = std::pair<TargetBlock *, TargetBlock *>;
  std::map<Edge, std::vector<Copy>> edge_copies;
  std::vector<Edge> edge_order;
  bool found_phi = false;
  for (auto *successor : func->GetBlocks()) {
    std::vector<TargetInst *> kept;
    for (auto *inst : successor->GetInsts()) {
      if (static_cast<RISCVInst *>(inst)->GetOpcode() != RVOpcode::PHI) {
        kept.push_back(inst);
        continue;
      }
      found_phi = true;
      auto *destination = inst->GetResult();
      const auto &operands = inst->GetOperands();
      for (size_t index = 0; index + 1 < operands.size(); index += 2) {
        auto *label = static_cast<TargetLabel *>(operands[index + 1]);
        auto pred_it = by_label.find(label->GetName());
        if (pred_it != by_label.end()) {
          Edge edge{pred_it->second, successor};
          if (edge_copies.find(edge) == edge_copies.end()) {
            edge_order.push_back(edge);
          }
          edge_copies[edge].emplace_back(destination, operands[index]);
        }
      }
    }
    successor->SetInsts(std::move(kept));
  }
  if (!found_phi) {
    return false;
  }

  std::vector<TargetBlock *> blocks = func->GetBlocks();
  for (const auto &edge : edge_order) {
    auto &copies = edge_copies[edge];
    auto *predecessor = edge.first;
    auto *successor = edge.second;
    auto targets = BranchTargets(predecessor);
    if (targets.size() <= 1) {
      EmitParallelCopies(predecessor, copies);
      continue;
    }

    // Copies for one successor of a multi-successor predecessor belong on that
    // edge only. Split it and redirect every matching explicit target.
    auto *split = ctx_->NewBlock(next_block_id++);
    split->SetLabel(".L_" + func->GetName() + "_phi_" + std::to_string(split->GetID()));
    EmitParallelCopies(split, copies);
    auto *jump = ctx_->CreateInst<RISCVInst>(RVOpcode::J);
    jump->AddOperand(ctx_->Label(successor->GetLabel()));
    split->AddInst(jump);

    for (auto *inst : predecessor->GetInsts()) {
      std::vector<TargetOperand *> operands = inst->GetOperands();
      bool changed = false;
      for (auto *&operand : operands) {
        if (operand->GetOpType() == TargetOperandType::Label &&
            static_cast<TargetLabel *>(operand)->GetName() == successor->GetLabel()) {
          operand = ctx_->Label(split->GetLabel());
          changed = true;
        }
      }
      if (changed) {
        inst->SetOperands(std::move(operands));
      }
    }
    blocks.push_back(split);
  }
  func->SetBlocks(std::move(blocks));
  return true;
}

}  // namespace backend::targetir::riscv
