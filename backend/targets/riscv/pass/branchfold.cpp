#include "backend/targets/riscv/pass/branchfold.h"
#include <unordered_map>
#include <vector>
#include "backend/targets/riscv/inst/riscvinst.h"
#include "backend/targets/utils/operand/targetsymbol.h"
#include "backend/targets/utils/struct/targetblock.h"
#include "backend/targets/utils/struct/targetfunction.h"

namespace backend::targetir::riscv {
namespace {
auto InverseBranch(RVOpcode opcode, RVOpcode *inverse) -> bool {
  switch (opcode) {
    case RVOpcode::BEQ: *inverse = RVOpcode::BNE; return true;
    case RVOpcode::BNE: *inverse = RVOpcode::BEQ; return true;
    case RVOpcode::BLT: *inverse = RVOpcode::BGE; return true;
    case RVOpcode::BGE: *inverse = RVOpcode::BLT; return true;
    case RVOpcode::BLTU: *inverse = RVOpcode::BGEU; return true;
    case RVOpcode::BGEU: *inverse = RVOpcode::BLTU; return true;
    default: return false;
  }
}
}  // namespace

auto BranchFold::RunOnFunction(TargetFunction *func) -> bool {
  const auto &blocks = func->GetBlocks();
  bool changed = false;
  std::unordered_map<std::string, TargetBlock *> by_label;
  for (auto *block : blocks) {
    by_label[block->GetLabel()] = block;
  }

  // Thread branches through empty forwarding blocks. Phi destruction and CFG
  // cleanup commonly leave chains such as `j merge.edge; merge.edge: j merge`;
  // following the chain once here avoids executing every intermediate jump.
  std::unordered_map<TargetBlock *, TargetOperand *> forwarding_cache;
  auto resolve_forwarder = [&](TargetOperand *target) -> TargetOperand * {
    std::vector<TargetBlock *> path;
    std::unordered_map<TargetBlock *, size_t> path_index;
    while (target != nullptr && target->GetOpType() == TargetOperandType::Label) {
      auto iter = by_label.find(static_cast<TargetLabel *>(target)->GetName());
      if (iter == by_label.end()) {
        break;
      }
      auto cached = forwarding_cache.find(iter->second);
      if (cached != forwarding_cache.end()) {
        target = cached->second;
        break;
      }
      if (path_index.count(iter->second) > 0) {
        break;
      }
      const auto &instructions = iter->second->GetInsts();
      if (instructions.size() != 1) {
        break;
      }
      auto *jump = static_cast<RISCVInst *>(instructions.front());
      const auto &operands = jump->GetOperands();
      if (jump->GetOpcode() != RVOpcode::J || operands.size() != 1 ||
          operands[0]->GetOpType() != TargetOperandType::Label) {
        break;
      }
      path_index[iter->second] = path.size();
      path.emplace_back(iter->second);
      target = operands[0];
    }
    for (auto *forwarder : path) {
      forwarding_cache[forwarder] = target;
    }
    return target;
  };

  for (size_t i = 0; i + 1 < blocks.size(); ++i) {
    auto *block = blocks[i];
    const auto &insts = block->GetInsts();
    if (insts.empty()) {
      continue;
    }

    // Instruction selection emits a conditional branch to the true block and
    // an unconditional jump to the false block. When the true block is already
    // next in layout order, invert the condition and branch directly to false;
    // true then falls through and the unconditional jump disappears.
    if (insts.size() >= 2) {
      auto *branch = static_cast<RISCVInst *>(insts[insts.size() - 2]);
      auto *jump = static_cast<RISCVInst *>(insts.back());
      RVOpcode inverse = RVOpcode::BEQ;
      const auto &branch_ops = branch->GetOperands();
      const auto &jump_ops = jump->GetOperands();
      if (jump->GetOpcode() == RVOpcode::J && InverseBranch(branch->GetOpcode(), &inverse) &&
          branch_ops.size() == 3 && jump_ops.size() == 1 &&
          branch_ops[2]->GetOpType() == TargetOperandType::Label &&
          jump_ops[0]->GetOpType() == TargetOperandType::Label &&
          static_cast<TargetLabel *>(branch_ops[2])->GetName() == blocks[i + 1]->GetLabel()) {
        branch->SetOpcode(inverse);
        branch->SetOperands({branch_ops[0], branch_ops[1], jump_ops[0]});
        std::vector<TargetInst *> kept(insts.begin(), insts.end() - 1);
        block->SetInsts(std::move(kept));
        changed = true;
        continue;
      }
    }

    auto *last = static_cast<RISCVInst *>(insts.back());
    if (last->GetOpcode() != RVOpcode::J) {
      continue;
    }
    const auto &ops = last->GetOperands();
    if (ops.empty() || ops[0]->GetOpType() != TargetOperandType::Label) {
      continue;
    }
    // The jump targets the very next block in layout order: it is redundant.
    if (static_cast<TargetLabel *>(ops[0])->GetName() == blocks[i + 1]->GetLabel()) {
      std::vector<TargetInst *> kept(insts.begin(), insts.end() - 1);
      block->SetInsts(kept);
      changed = true;
    }
  }

  for (auto *block : blocks) {
    const auto &instructions = block->GetInsts();
    for (auto *instruction : instructions) {
      auto *branch = static_cast<RISCVInst *>(instruction);
      auto operands = branch->GetOperands();
      if (branch->GetOpcode() == RVOpcode::J && operands.size() == 1) {
        auto *target = resolve_forwarder(operands[0]);
        if (target != operands[0]) {
          branch->SetOperands({target});
          changed = true;
        }
        continue;
      }
      RVOpcode unused = RVOpcode::BEQ;
      if (InverseBranch(branch->GetOpcode(), &unused) && operands.size() == 3) {
        auto *target = resolve_forwarder(operands[2]);
        if (target != operands[2]) {
          branch->SetOperands({operands[0], operands[1], target});
          changed = true;
        }
      }
    }
  }

  // Threading can expose one more direct fallthrough.
  for (size_t i = 0; i + 1 < blocks.size(); ++i) {
    const auto &instructions = blocks[i]->GetInsts();
    if (instructions.empty()) {
      continue;
    }
    auto *last = static_cast<RISCVInst *>(instructions.back());
    const auto &operands = last->GetOperands();
    if (last->GetOpcode() == RVOpcode::J && operands.size() == 1 &&
        operands[0]->GetOpType() == TargetOperandType::Label &&
        static_cast<TargetLabel *>(operands[0])->GetName() == blocks[i + 1]->GetLabel()) {
      std::vector<TargetInst *> kept(instructions.begin(), instructions.end() - 1);
      blocks[i]->SetInsts(std::move(kept));
      changed = true;
    }
  }
  return changed;
}

}  // namespace backend::targetir::riscv
