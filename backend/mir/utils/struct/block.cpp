#include "backend/mir/utils/struct/block.h"
#include <queue>
#include "backend/mir/inst/inst.h"

namespace backend::utils {
Block::~Block() { instructions_.clear(); }

auto Block::Emit(std::ofstream *out) const -> void {
  *out << "block" << id_ << ":\n";
  for (const auto &inst : instructions_) {
    inst->Emit(out);
  }
}

auto Block::AddInstBeforeTerminal(inst::Instruction *inst) -> void {
  if (instructions_.empty()) {
    instructions_.emplace_back(inst);
    return;
  }
  std::queue<inst::Instruction *> tmp_insts;
  while (instructions_.back()->IsTerminal()) {
    tmp_insts.push(instructions_.back());
    instructions_.pop_back();
    if (instructions_.empty()) {
      break;
    }
  }
  instructions_.emplace_back(inst);
  while (!tmp_insts.empty()) {
    instructions_.emplace_back(tmp_insts.front());
    tmp_insts.pop();
  }
}

auto Block::RemoveInst(inst::Instruction *inst) -> void {
  std::deque<inst::Instruction *> new_insts;
  for (const auto &i : instructions_) {
    if (i != inst) {
      new_insts.emplace_back(i);
    }
  }
  instructions_ = new_insts;
}

auto Block::RemovePhiInst() -> void {
  while (instructions_.front()->IsPhi()) {
    instructions_.pop_front();
    if (instructions_.empty()) {
      break;
    }
  }
}

}  // namespace backend::utils