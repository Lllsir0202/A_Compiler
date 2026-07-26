#include "backend/targets/riscv/pass/machinescheduler.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "backend/targets/riscv/inst/riscvinst.h"
#include "backend/targets/utils/inst/targetinst.h"
#include "backend/targets/utils/operand/targetoperand.h"
#include "backend/targets/utils/operand/targetreg.h"
#include "backend/targets/utils/struct/targetblock.h"
#include "backend/targets/utils/struct/targetfunction.h"

namespace backend::targetir::riscv {

namespace {

auto IsRegister(TargetOperand *operand) -> bool {
  return operand != nullptr && (operand->IsVirtualReg() || operand->IsPhysicalReg());
}

auto Latency(TargetInst *instruction) -> int {
  auto opcode = static_cast<RISCVInst *>(instruction)->GetOpcode();
  switch (opcode) {
    case RVOpcode::LW:
    case RVOpcode::LD:
    case RVOpcode::FLW:
      return 4;
    case RVOpcode::MULW:
    case RVOpcode::MUL:
      return 3;
    case RVOpcode::DIVW:
    case RVOpcode::REMW:
      return 16;
    case RVOpcode::FADD_S:
    case RVOpcode::FSUB_S:
      return 3;
    case RVOpcode::FMUL_S:
      return 4;
    case RVOpcode::FDIV_S:
      return 16;
    case RVOpcode::FCVT_S_W:
    case RVOpcode::FCVT_W_S:
      return 4;
    default:
      return 1;
  }
}

auto IsPinned(TargetInst *instruction) -> bool {
  auto opcode = static_cast<RISCVInst *>(instruction)->GetOpcode();
  const auto &info = GetRVOpcodeInfo(opcode);
  if (opcode == RVOpcode::COMMENT || opcode == RVOpcode::PHI) {
    return true;
  }
  if (info.HasFlag(RVInstFlag::Terminator) || info.HasFlag(RVInstFlag::Call) || info.HasFlag(RVInstFlag::Pseudo)) {
    return true;
  }
  // Stores participate in the conservative memory chain. Any other
  // side-effecting instruction is a region boundary until its effects are
  // represented explicitly in the dependence graph.
  return info.HasFlag(RVInstFlag::SideEffect) && !info.HasFlag(RVInstFlag::MayStore);
}

struct ScheduleNode {
  TargetInst *instruction_ = nullptr;
  size_t original_index_ = 0;
  int indegree_ = 0;
  int latency_ = 1;
  int height_ = 1;
  std::vector<size_t> successors_;
  std::unordered_set<size_t> successor_set_;
};

struct PressureMetric {
  int gpr_peak_ = 0;
  int fpr_peak_ = 0;
  int gpr_length_ = 0;
  int fpr_length_ = 0;
  std::set<std::pair<size_t, size_t>> interferences_;
};

auto ValueKey(TargetOperand *operand) -> size_t {
  auto *reg = static_cast<TargetVirtualReg *>(operand);
  return reg->GetID() * 2 + (reg->GetRegClass() == TargetRegClass::Float ? 1 : 0);
}

auto MeasurePressure(const std::vector<TargetInst *> &order,
                     const std::unordered_map<TargetOperand *, int> &global_uses) -> PressureMetric {
  struct Interval {
    int begin_ = 0;
    int end_ = 0;
    int local_uses_ = 0;
    TargetRegClass class_{};
  };
  std::unordered_map<TargetOperand *, Interval> intervals;
  for (size_t index = 0; index < order.size(); ++index) {
    auto *instruction = order[index];
    for (auto *operand : instruction->GetOperands()) {
      if (operand == nullptr || !operand->IsVirtualReg()) {
        continue;
      }
      auto *reg = static_cast<TargetVirtualReg *>(operand);
      auto iter = intervals.try_emplace(operand, Interval{0, static_cast<int>(index), 0, reg->GetRegClass()}).first;
      iter->second.end_ = std::max(iter->second.end_, static_cast<int>(index));
      iter->second.local_uses_++;
    }
    auto *result = instruction->GetResult();
    if (result != nullptr && result->IsVirtualReg()) {
      auto *reg = static_cast<TargetVirtualReg *>(result);
      auto [iter, inserted] =
          intervals.emplace(result, Interval{static_cast<int>(index), static_cast<int>(index), 0, reg->GetRegClass()});
      if (!inserted) {
        iter->second.begin_ = static_cast<int>(index);
      }
    }
  }

  PressureMetric metric;
  std::vector<int> gpr_live(order.size() + 1, 0);
  std::vector<int> fpr_live(order.size() + 1, 0);
  for (auto &[operand, interval] : intervals) {
    auto global = global_uses.find(operand);
    if (global != global_uses.end() && global->second > interval.local_uses_) {
      interval.end_ = static_cast<int>(order.size());
    }
    int length = std::max(0, interval.end_ - interval.begin_);
    auto &live = interval.class_ == TargetRegClass::Float ? fpr_live : gpr_live;
    for (int slot = interval.begin_; slot <= interval.end_; ++slot) {
      live[static_cast<size_t>(slot)]++;
    }
    if (interval.class_ == TargetRegClass::Float) {
      metric.fpr_length_ += length;
    } else {
      metric.gpr_length_ += length;
    }
  }
  std::vector<std::pair<TargetOperand *, Interval>> interval_list(intervals.begin(), intervals.end());
  for (size_t lhs_index = 0; lhs_index < interval_list.size(); ++lhs_index) {
    for (size_t rhs_index = lhs_index + 1; rhs_index < interval_list.size(); ++rhs_index) {
      auto &[lhs_operand, lhs] = interval_list[lhs_index];
      auto &[rhs_operand, rhs] = interval_list[rhs_index];
      if (lhs.class_ != rhs.class_ || std::max(lhs.begin_, rhs.begin_) > std::min(lhs.end_, rhs.end_)) {
        continue;
      }
      size_t first = ValueKey(lhs_operand);
      size_t second = ValueKey(rhs_operand);
      if (second < first) {
        std::swap(first, second);
      }
      metric.interferences_.emplace(first, second);
    }
  }
  for (auto live : gpr_live) {
    metric.gpr_peak_ = std::max(metric.gpr_peak_, live);
  }
  for (auto live : fpr_live) {
    metric.fpr_peak_ = std::max(metric.fpr_peak_, live);
  }
  return metric;
}

auto ScheduleRegion(const std::vector<TargetInst *> &region,
                    const std::unordered_map<TargetOperand *, int> &global_uses,
                    std::vector<TargetInst *> *scheduled) -> bool {
  if (region.size() < 3) {
    scheduled->insert(scheduled->end(), region.begin(), region.end());
    return false;
  }

  std::vector<ScheduleNode> nodes(region.size());
  std::unordered_map<TargetOperand *, size_t> last_definition;
  std::unordered_map<TargetOperand *, std::vector<size_t>> physical_readers;
  std::unordered_map<TargetOperand *, int> remaining_uses;
  std::unordered_map<TargetOperand *, size_t> local_definition;
  size_t last_memory = region.size();

  auto add_edge = [&](size_t from, size_t to) {
    if (from == to || !nodes[from].successor_set_.insert(to).second) {
      return;
    }
    nodes[from].successors_.push_back(to);
    nodes[to].indegree_++;
  };

  for (size_t index = 0; index < region.size(); ++index) {
    auto *instruction = region[index];
    nodes[index].instruction_ = instruction;
    nodes[index].original_index_ = index;
    nodes[index].latency_ = Latency(instruction);

    for (auto *operand : instruction->GetOperands()) {
      if (!IsRegister(operand)) {
        continue;
      }
      remaining_uses[operand]++;
      auto definition = last_definition.find(operand);
      if (definition != last_definition.end()) {
        add_edge(definition->second, index);
      }
      if (operand->IsPhysicalReg()) {
        physical_readers[operand].push_back(index);
      }
    }

    auto *result = instruction->GetResult();
    if (IsRegister(result)) {
      auto definition = last_definition.find(result);
      if (definition != last_definition.end()) {
        add_edge(definition->second, index);
      }
      if (result->IsPhysicalReg()) {
        for (auto reader : physical_readers[result]) {
          add_edge(reader, index);
        }
        physical_readers[result].clear();
      }
      last_definition[result] = index;
      local_definition[result] = index;
    }

    const auto &info = GetRVOpcodeInfo(static_cast<RISCVInst *>(instruction)->GetOpcode());
    if (info.HasFlag(RVInstFlag::MayLoad) || info.HasFlag(RVInstFlag::MayStore)) {
      if (last_memory != region.size()) {
        add_edge(last_memory, index);
      }
      last_memory = index;
    }
  }

  // All dependence edges point forward in the original legal schedule.
  // Computing heights in reverse therefore gives the critical-path priority.
  for (size_t index = nodes.size(); index-- > 0;) {
    int successor_height = 0;
    for (auto successor : nodes[index].successors_) {
      successor_height = std::max(successor_height, nodes[successor].height_);
    }
    nodes[index].height_ = nodes[index].latency_ + successor_height;
  }

  std::vector<size_t> ready;
  for (size_t index = 0; index < nodes.size(); ++index) {
    if (nodes[index].indegree_ == 0) {
      ready.push_back(index);
    }
  }
  std::vector<int> issue_slot(nodes.size(), -1);
  std::vector<TargetInst *> order;
  order.reserve(nodes.size());

  for (int slot = 0; !ready.empty(); ++slot) {
    size_t best_position = 0;
    int best_wait = std::numeric_limits<int>::max();
    int best_score = std::numeric_limits<int>::min();
    size_t best_original = nodes.size();
    for (size_t position = 0; position < ready.size(); ++position) {
      size_t candidate = ready[position];
      int available_at = 0;
      int released_operands = 0;
      for (auto *operand : nodes[candidate].instruction_->GetOperands()) {
        if (!IsRegister(operand)) {
          continue;
        }
        auto definition = local_definition.find(operand);
        if (definition != local_definition.end() && issue_slot[definition->second] >= 0) {
          available_at = std::max(available_at, issue_slot[definition->second] + nodes[definition->second].latency_);
        }
        if (remaining_uses[operand] == 1) {
          released_operands++;
        }
      }
      int wait = std::max(0, available_at - slot);
      const auto &info = GetRVOpcodeInfo(static_cast<RISCVInst *>(nodes[candidate].instruction_)->GetOpcode());
      int score = nodes[candidate].height_ * 32 + released_operands * 3;
      if (info.HasFlag(RVInstFlag::MayLoad)) {
        score += 8;
      }
      if (info.HasFlag(RVInstFlag::Rematerializable)) {
        score -= 4;
      }
      if (wait < best_wait || (wait == best_wait && score > best_score) ||
          (wait == best_wait && score == best_score && nodes[candidate].original_index_ < best_original)) {
        best_position = position;
        best_wait = wait;
        best_score = score;
        best_original = nodes[candidate].original_index_;
      }
    }

    size_t selected = ready[best_position];
    ready.erase(ready.begin() + static_cast<std::ptrdiff_t>(best_position));
    issue_slot[selected] = slot;
    order.push_back(nodes[selected].instruction_);
    for (auto *operand : nodes[selected].instruction_->GetOperands()) {
      if (IsRegister(operand)) {
        remaining_uses[operand]--;
      }
    }
    for (auto successor : nodes[selected].successors_) {
      if (--nodes[successor].indegree_ == 0) {
        ready.push_back(successor);
      }
    }
  }

  // The original sequence is a legal topological order, so an incomplete
  // schedule indicates an internal dependence-graph bug. Preserve the input
  // rather than emitting a partial block.
  if (order.size() != region.size()) {
    scheduled->insert(scheduled->end(), region.begin(), region.end());
    return false;
  }
  auto original_pressure = MeasurePressure(region, global_uses);
  auto scheduled_pressure = MeasurePressure(order, global_uses);
  // Pre-RA scheduling must not buy latency by making allocation harder. Peak
  // pressure controls colorability; total interval length is a stable tie-break
  // that avoids trading many short ranges for a few long-lived values.
  if (scheduled_pressure.gpr_peak_ > original_pressure.gpr_peak_ ||
      scheduled_pressure.fpr_peak_ > original_pressure.fpr_peak_ ||
      scheduled_pressure.gpr_length_ > original_pressure.gpr_length_ ||
      scheduled_pressure.fpr_length_ > original_pressure.fpr_length_ ||
      !std::includes(original_pressure.interferences_.begin(), original_pressure.interferences_.end(),
                     scheduled_pressure.interferences_.begin(), scheduled_pressure.interferences_.end())) {
    scheduled->insert(scheduled->end(), region.begin(), region.end());
    return false;
  }
  bool changed = order != region;
  scheduled->insert(scheduled->end(), order.begin(), order.end());
  return changed;
}

}  // namespace

auto MachineScheduler::RunOnFunction(TargetFunction *function) -> bool {
  bool changed = false;
  std::unordered_map<TargetOperand *, int> global_uses;
  for (auto *block : function->GetBlocks()) {
    for (auto *instruction : block->GetInsts()) {
      for (auto *operand : instruction->GetOperands()) {
        if (operand != nullptr && operand->IsVirtualReg()) {
          global_uses[operand]++;
        }
      }
    }
  }
  for (const auto &[edge, instructions] : function->GetAllEdgeCode()) {
    (void)edge;
    for (auto *instruction : instructions) {
      for (auto *operand : instruction->GetOperands()) {
        if (operand != nullptr && operand->IsVirtualReg()) {
          global_uses[operand]++;
        }
      }
    }
  }
  for (auto *block : function->GetBlocks()) {
    std::vector<TargetInst *> rewritten;
    std::vector<TargetInst *> region;
    auto flush_region = [&]() {
      changed |= ScheduleRegion(region, global_uses, &rewritten);
      region.clear();
    };
    for (auto *instruction : block->GetInsts()) {
      if (IsPinned(instruction)) {
        flush_region();
        rewritten.push_back(instruction);
      } else {
        region.push_back(instruction);
      }
    }
    flush_region();
    block->SetInsts(std::move(rewritten));
  }
  return changed;
}

}  // namespace backend::targetir::riscv
