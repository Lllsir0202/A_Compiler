#include "backend/targets/riscv/pass/machineanalysis.h"

#include <algorithm>
#include <string>
#include <utility>
#include "backend/targets/riscv/inst/riscvinst.h"
#include "backend/targets/utils/operand/targetreg.h"
#include "backend/targets/utils/operand/targetsymbol.h"
#include "backend/targets/utils/struct/targetblock.h"
#include "backend/targets/utils/struct/targetfunction.h"

namespace backend::targetir::riscv {

namespace {

auto AppendUnique(std::vector<TargetBlock *> *blocks, TargetBlock *block) -> void {
  if (block != nullptr && std::find(blocks->begin(), blocks->end(), block) == blocks->end()) {
    blocks->push_back(block);
  }
}

auto IsUnconditionalTransfer(RVOpcode opcode) -> bool { return opcode == RVOpcode::J || opcode == RVOpcode::RET; }

}  // namespace

MachineCFG::MachineCFG(TargetFunction *function) : function_(function) {
  const auto &blocks = function_->GetBlocks();
  for (size_t index = 0; index < blocks.size(); ++index) {
    index_[blocks[index]] = index;
    by_label_[blocks[index]->GetLabel()] = blocks[index];
  }
  BuildEdges();
  ComputeDominators();
  FindNaturalLoops();
}

auto MachineCFG::Blocks() const -> const std::vector<TargetBlock *> & { return function_->GetBlocks(); }

auto MachineCFG::Successors(TargetBlock *block) const -> const std::vector<TargetBlock *> & {
  static const std::vector<TargetBlock *> EMPTY;
  auto iter = successors_.find(block);
  return iter == successors_.end() ? EMPTY : iter->second;
}

auto MachineCFG::Predecessors(TargetBlock *block) const -> const std::vector<TargetBlock *> & {
  static const std::vector<TargetBlock *> EMPTY;
  auto iter = predecessors_.find(block);
  return iter == predecessors_.end() ? EMPTY : iter->second;
}

auto MachineCFG::BlockForLabel(const std::string &label) const -> TargetBlock * {
  auto iter = by_label_.find(label);
  return iter == by_label_.end() ? nullptr : iter->second;
}

auto MachineCFG::ContainsLabel(const std::string &label) const -> bool { return by_label_.count(label) != 0; }

auto MachineCFG::Dominates(TargetBlock *dominator, TargetBlock *block) const -> bool {
  auto dom_iter = index_.find(dominator);
  auto block_iter = index_.find(block);
  return dom_iter != index_.end() && block_iter != index_.end() &&
         dominators_[block_iter->second].count(dom_iter->second) != 0;
}

auto MachineCFG::DominatorChildren(TargetBlock *block) const -> const std::vector<TargetBlock *> & {
  static const std::vector<TargetBlock *> EMPTY;
  auto iter = dom_children_.find(block);
  return iter == dom_children_.end() ? EMPTY : iter->second;
}

auto MachineCFG::LoopDepth(TargetBlock *block) const -> int {
  auto iter = loop_depth_.find(block);
  return iter == loop_depth_.end() ? 0 : iter->second;
}

auto MachineCFG::BuildEdges() -> void {
  const auto &blocks = function_->GetBlocks();
  for (size_t index = 0; index < blocks.size(); ++index) {
    auto *block = blocks[index];
    auto &successors = successors_[block];
    const auto &instructions = block->GetInsts();
    for (auto *instruction : instructions) {
      const auto &info = GetRVOpcodeInfo(static_cast<RISCVInst *>(instruction)->GetOpcode());
      if (!info.HasFlag(RVInstFlag::Branch)) {
        continue;
      }
      for (auto *operand : instruction->GetOperands()) {
        if (operand->GetOpType() == TargetOperandType::Label) {
          AppendUnique(&successors, BlockForLabel(static_cast<TargetLabel *>(operand)->GetName()));
        }
      }
    }
    bool unconditional =
        !instructions.empty() && IsUnconditionalTransfer(static_cast<RISCVInst *>(instructions.back())->GetOpcode());
    if (!unconditional && index + 1 < blocks.size()) {
      AppendUnique(&successors, blocks[index + 1]);
    }
  }
  for (auto *block : blocks) {
    predecessors_[block];
    for (auto *successor : successors_[block]) {
      AppendUnique(&predecessors_[successor], block);
    }
  }
}

auto MachineCFG::ComputeDominators() -> void {
  const auto &blocks = function_->GetBlocks();
  const size_t count = blocks.size();
  dominators_.assign(count, {});
  if (count == 0) {
    return;
  }
  std::unordered_set<size_t> all;
  for (size_t index = 0; index < count; ++index) {
    all.insert(index);
  }
  dominators_[0] = {0};
  for (size_t index = 1; index < count; ++index) {
    dominators_[index] = Predecessors(blocks[index]).empty() ? std::unordered_set<size_t>{index} : all;
  }

  bool changed = true;
  while (changed) {
    changed = false;
    for (size_t index = 1; index < count; ++index) {
      const auto &predecessors = Predecessors(blocks[index]);
      std::unordered_set<size_t> next;
      if (!predecessors.empty()) {
        next = dominators_[index_[predecessors.front()]];
        for (size_t pred_index = 1; pred_index < predecessors.size(); ++pred_index) {
          std::unordered_set<size_t> intersection;
          const auto &pred_dom = dominators_[index_[predecessors[pred_index]]];
          for (size_t candidate : next) {
            if (pred_dom.count(candidate) != 0) {
              intersection.insert(candidate);
            }
          }
          next = std::move(intersection);
        }
      }
      next.insert(index);
      if (next != dominators_[index]) {
        dominators_[index] = std::move(next);
        changed = true;
      }
    }
  }

  for (size_t block_index = 1; block_index < count; ++block_index) {
    TargetBlock *idom = nullptr;
    for (size_t candidate : dominators_[block_index]) {
      if (candidate == block_index) {
        continue;
      }
      bool closest = true;
      for (size_t other : dominators_[block_index]) {
        if (other == block_index || other == candidate) {
          continue;
        }
        if (dominators_[candidate].count(other) == 0) {
          closest = false;
          break;
        }
      }
      if (closest) {
        idom = blocks[candidate];
        break;
      }
    }
    if (idom != nullptr) {
      dom_children_[idom].push_back(blocks[block_index]);
    }
  }
}

auto MachineCFG::FindNaturalLoops() -> void {
  const auto &blocks = function_->GetBlocks();
  std::unordered_map<TargetBlock *, size_t> loop_for_header;
  for (auto *latch : blocks) {
    for (auto *header : Successors(latch)) {
      if (!Dominates(header, latch)) {
        continue;
      }
      size_t loop_index = 0;
      auto found = loop_for_header.find(header);
      if (found == loop_for_header.end()) {
        loop_index = loops_.size();
        loop_for_header[header] = loop_index;
        loops_.push_back(MachineNaturalLoop{header, nullptr, {header}});
      } else {
        loop_index = found->second;
      }
      auto &loop = loops_[loop_index];
      std::vector<TargetBlock *> work;
      if (loop.blocks_.insert(latch).second && latch != header) {
        work.push_back(latch);
      }
      while (!work.empty()) {
        auto *current = work.back();
        work.pop_back();
        for (auto *predecessor : Predecessors(current)) {
          if (loop.blocks_.insert(predecessor).second && predecessor != header) {
            work.push_back(predecessor);
          }
        }
      }
    }
  }

  for (auto &loop : loops_) {
    for (auto *predecessor : Predecessors(loop.header_)) {
      if (loop.blocks_.count(predecessor) != 0) {
        continue;
      }
      if (loop.preheader_ != nullptr) {
        loop.preheader_ = nullptr;
        break;
      }
      loop.preheader_ = predecessor;
    }
    for (auto *block : loop.blocks_) {
      loop_depth_[block]++;
    }
  }
  std::stable_sort(loops_.begin(), loops_.end(), [](const MachineNaturalLoop &lhs, const MachineNaturalLoop &rhs) {
    return lhs.blocks_.size() < rhs.blocks_.size();
  });
}

auto MachineLiveness::EdgeHash::operator()(const Edge &edge) const -> size_t {
  auto lhs = reinterpret_cast<size_t>(edge.first);
  auto rhs = reinterpret_cast<size_t>(edge.second);
  return lhs ^ (rhs + 0x9e3779b97f4a7c15ULL + (lhs << 6U) + (lhs >> 2U));
}

MachineLiveness::MachineLiveness(TargetFunction *function, const MachineCFG &cfg) : function_(function), cfg_(cfg) {
  BuildLocalSets();
  Solve();
}

auto MachineLiveness::LiveIn(TargetBlock *block) const -> const RegSet & {
  static const RegSet EMPTY;
  auto iter = live_in_.find(block);
  return iter == live_in_.end() ? EMPTY : iter->second;
}

auto MachineLiveness::LiveOut(TargetBlock *block) const -> const RegSet & {
  static const RegSet EMPTY;
  auto iter = live_out_.find(block);
  return iter == live_out_.end() ? EMPTY : iter->second;
}

auto MachineLiveness::EdgeUses(TargetBlock *predecessor, TargetBlock *successor) const -> const RegSet & {
  static const RegSet EMPTY;
  auto iter = edge_uses_.find({predecessor, successor});
  return iter == edge_uses_.end() ? EMPTY : iter->second;
}

auto MachineLiveness::IsVirtualClass(TargetOperand *operand, TargetRegClass reg_class) -> bool {
  return operand != nullptr && operand->IsVirtualReg() &&
         static_cast<TargetVirtualReg *>(operand)->GetRegClass() == reg_class;
}

auto MachineLiveness::BuildLocalSets() -> void {
  for (auto *block : function_->GetBlocks()) {
    auto &uses = uses_[block];
    auto &defs = defs_[block];
    for (auto *instruction : block->GetInsts()) {
      auto opcode = static_cast<RISCVInst *>(instruction)->GetOpcode();
      if (opcode == RVOpcode::PHI) {
        auto *result = instruction->GetResult();
        if (result != nullptr && result->IsVirtualReg()) {
          defs.insert(result);
        }
        const auto &operands = instruction->GetOperands();
        for (size_t index = 0; index + 1 < operands.size(); index += 2) {
          auto *predecessor = cfg_.BlockForLabel(static_cast<TargetLabel *>(operands[index + 1])->GetName());
          if (predecessor != nullptr && operands[index]->IsVirtualReg()) {
            edge_uses_[{predecessor, block}].insert(operands[index]);
          }
        }
        continue;
      }
      for (auto *operand : instruction->GetOperands()) {
        if (operand->IsVirtualReg() && defs.count(operand) == 0) {
          uses.insert(operand);
        }
      }
      auto *result = instruction->GetResult();
      if (result != nullptr && result->IsVirtualReg()) {
        defs.insert(result);
      }
    }
    live_in_[block];
    live_out_[block];
  }
}

auto MachineLiveness::Solve() -> void {
  const auto &blocks = function_->GetBlocks();
  bool changed = true;
  while (changed) {
    changed = false;
    for (auto iter = blocks.rbegin(); iter != blocks.rend(); ++iter) {
      auto *block = *iter;
      RegSet out;
      for (auto *successor : cfg_.Successors(block)) {
        const auto &successor_live = LiveIn(successor);
        out.insert(successor_live.begin(), successor_live.end());
        const auto &edge_uses = EdgeUses(block, successor);
        out.insert(edge_uses.begin(), edge_uses.end());
      }
      RegSet in = uses_[block];
      for (auto *reg : out) {
        if (defs_[block].count(reg) == 0) {
          in.insert(reg);
        }
      }
      if (out != live_out_[block] || in != live_in_[block]) {
        live_out_[block] = std::move(out);
        live_in_[block] = std::move(in);
        changed = true;
      }
    }
  }
}

auto MachineLiveness::PeakPressure(const std::unordered_set<TargetBlock *> &blocks,
                                   TargetRegClass reg_class) const -> int {
  int peak = 0;
  for (auto *block : blocks) {
    RegSet live;
    for (auto *reg : LiveOut(block)) {
      if (IsVirtualClass(reg, reg_class)) {
        live.insert(reg);
      }
    }
    peak = std::max(peak, static_cast<int>(live.size()));
    const auto &instructions = block->GetInsts();
    for (auto iter = instructions.rbegin(); iter != instructions.rend(); ++iter) {
      auto *instruction = *iter;
      auto *result = instruction->GetResult();
      if (IsVirtualClass(result, reg_class)) {
        live.erase(result);
      }
      if (static_cast<RISCVInst *>(instruction)->GetOpcode() != RVOpcode::PHI) {
        for (auto *operand : instruction->GetOperands()) {
          if (IsVirtualClass(operand, reg_class)) {
            live.insert(operand);
          }
        }
      }
      peak = std::max(peak, static_cast<int>(live.size()));
    }
  }
  return peak;
}

}  // namespace backend::targetir::riscv
