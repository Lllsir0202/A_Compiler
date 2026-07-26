#include "backend/targets/riscv/pass/machinephielim.h"

#include <algorithm>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "backend/targets/riscv/inst/riscvinst.h"
#include "backend/targets/utils/context/targetcontext.h"
#include "backend/targets/utils/operand/targetreg.h"
#include "backend/targets/utils/operand/targetsymbol.h"
#include "backend/targets/utils/struct/targetblock.h"
#include "backend/targets/utils/struct/targetfunction.h"

namespace backend::targetir::riscv {

namespace {

auto CopyOpcode(TargetOperand *destination) -> RVOpcode {
  auto *reg = static_cast<TargetVirtualReg *>(destination);
  return reg->GetRegClass() == TargetRegClass::Float ? RVOpcode::FMV_S : RVOpcode::MV;
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
    if (copy.first != copy.second) {
      work.push_back(copy);
    }
  }
  std::vector<TargetInst *> emitted;
  while (!work.empty()) {
    auto ready = work.end();
    for (auto it = work.begin(); it != work.end(); ++it) {
      bool destination_is_source = false;
      for (const auto &other : work) {
        if (other.second == it->first) {
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
      auto *move = ctx_->CreateInst<RISCVInst>(CopyOpcode(ready->first));
      move->SetResult(ready->first);
      move->AddOperand(ready->second);
      emitted.push_back(move);
      work.erase(ready);
      continue;
    }

    // A parallel-copy cycle. Preserve one old destination and redirect every
    // reader of that old value to the temporary.
    auto *destination = static_cast<TargetVirtualReg *>(work.front().first);
    auto *temporary = ctx_->NewVirtualReg(destination->GetRegClass(), destination->GetRegSize());
    auto *save = ctx_->CreateInst<RISCVInst>(CopyOpcode(destination));
    save->SetResult(temporary);
    save->AddOperand(destination);
    emitted.push_back(save);
    for (auto &copy : work) {
      if (copy.second == destination) {
        copy.second = temporary;
      }
    }
  }
  InsertBeforeTerminators(block, emitted);
}

auto MachinePhiElimination::RunOnFunction(TargetFunction *func) -> bool {
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
    const auto &edge_code = func->GetEdgeCode(predecessor->GetLabel(), successor->GetLabel());
    auto targets = BranchTargets(predecessor);
    if (targets.size() <= 1) {
      InsertBeforeTerminators(predecessor, edge_code);
      EmitParallelCopies(predecessor, copies);
      continue;
    }

    // Copies for one successor of a multi-successor predecessor belong on that
    // edge only. Split it and redirect every matching explicit target.
    auto *split = ctx_->NewBlock(next_block_id++);
    split->SetLabel(".L_" + func->GetName() + "_phi_" + std::to_string(split->GetID()));
    InsertBeforeTerminators(split, edge_code);
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
