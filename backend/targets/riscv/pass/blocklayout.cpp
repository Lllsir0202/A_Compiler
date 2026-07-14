#include "backend/targets/riscv/pass/blocklayout.h"
#include <algorithm>
#include <unordered_map>
#include <vector>
#include "backend/targets/riscv/inst/riscvinst.h"
#include "backend/targets/utils/operand/targetsymbol.h"
#include "backend/targets/utils/struct/targetblock.h"
#include "backend/targets/utils/struct/targetfunction.h"

namespace backend::targetir::riscv {
namespace {
auto IsConditional(RVOpcode opcode) -> bool {
  return opcode == RVOpcode::BEQ || opcode == RVOpcode::BNE || opcode == RVOpcode::BLT ||
         opcode == RVOpcode::BGE || opcode == RVOpcode::BLTU || opcode == RVOpcode::BGEU;
}

auto LabelTarget(TargetInst *instruction) -> std::string {
  const auto &operands = instruction->GetOperands();
  if (operands.empty() || operands.back()->GetOpType() != TargetOperandType::Label) {
    return {};
  }
  return static_cast<TargetLabel *>(operands.back())->GetName();
}
}  // namespace

auto RISCVBlockLayout::RunOnFunction(TargetFunction *func) -> bool {
  if (processed_.count(func) > 0) {
    return false;
  }
  processed_.insert(func);
  const auto original = func->GetBlocks();
  if (original.size() < 2) {
    return false;
  }

  std::unordered_map<std::string, TargetBlock *> by_label;
  for (auto *block : original) {
    by_label[block->GetLabel()] = block;
  }

  auto preferred_successors = [&](TargetBlock *block) {
    std::vector<TargetBlock *> result;
    const auto &instructions = block->GetInsts();
    auto append_label = [&](TargetInst *instruction) {
      auto iter = by_label.find(LabelTarget(instruction));
      if (iter != by_label.end() && std::find(result.begin(), result.end(), iter->second) == result.end()) {
        result.emplace_back(iter->second);
      }
    };
    if (instructions.size() >= 2) {
      auto *conditional = static_cast<RISCVInst *>(instructions[instructions.size() - 2]);
      auto *jump = static_cast<RISCVInst *>(instructions.back());
      if (IsConditional(conditional->GetOpcode()) && jump->GetOpcode() == RVOpcode::J) {
        append_label(conditional);  // favor the true/body edge as fallthrough
        append_label(jump);
      }
    }
    if (result.empty() && !instructions.empty()) {
      auto *last = static_cast<RISCVInst *>(instructions.back());
      if (last->GetOpcode() == RVOpcode::J || IsConditional(last->GetOpcode())) {
        append_label(last);
      }
    }
    for (auto *successor : block->GetSuccessors()) {
      if (std::find(result.begin(), result.end(), successor) == result.end()) {
        result.emplace_back(successor);
      }
    }
    return result;
  };

  std::vector<TargetBlock *> layout;
  layout.reserve(original.size());
  std::unordered_set<TargetBlock *> placed;
  for (auto *seed : original) {
    TargetBlock *current = seed;
    while (current != nullptr && placed.count(current) == 0) {
      layout.emplace_back(current);
      placed.insert(current);
      TargetBlock *next = nullptr;
      for (auto *successor : preferred_successors(current)) {
        if (placed.count(successor) == 0) {
          next = successor;
          break;
        }
      }
      current = next;
    }
  }

  if (layout == original) {
    return false;
  }
  func->SetBlocks(std::move(layout));
  return true;
}

}  // namespace backend::targetir::riscv
