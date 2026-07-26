#include "backend/targets/riscv/pass/localcse.h"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "backend/targets/riscv/inst/riscvinst.h"
#include "backend/targets/riscv/pass/machineanalysis.h"
#include "backend/targets/utils/operand/targetliteral.h"
#include "backend/targets/utils/operand/targetreg.h"
#include "backend/targets/utils/operand/targetsymbol.h"
#include "backend/targets/utils/struct/targetblock.h"
#include "backend/targets/utils/struct/targetfunction.h"

namespace backend::targetir::riscv {

namespace {

auto OperandKey(TargetOperand *operand) -> std::string {
  switch (operand->GetOpType()) {
    case TargetOperandType::VirtualReg:
      return "v" + std::to_string(static_cast<TargetVirtualReg *>(operand)->GetID());
    case TargetOperandType::Immediate:
      return "i" + std::to_string(static_cast<TargetIntLiteral *>(operand)->GetValue());
    case TargetOperandType::Symbol:
      return "s" + static_cast<TargetSymbol *>(operand)->GetName();
    case TargetOperandType::Label:
      return "l" + static_cast<TargetLabel *>(operand)->GetName();
    default:
      // Values read from physical registers are not stable SSA values.
      return {};
  }
}

auto Resolve(TargetOperand *operand,
             const std::unordered_map<TargetOperand *, TargetOperand *> &replacements) -> TargetOperand * {
  auto iter = replacements.find(operand);
  while (iter != replacements.end()) {
    operand = iter->second;
    iter = replacements.find(operand);
  }
  return operand;
}

}  // namespace

auto RISCVLocalCSE::KeyOf(TargetInst *instruction) -> std::string {
  auto opcode = static_cast<RISCVInst *>(instruction)->GetOpcode();
  const auto &info = GetRVOpcodeInfo(opcode);
  if (!info.IsPure()) {
    return {};
  }
  auto *result = instruction->GetResult();
  if (result == nullptr || !result->IsVirtualReg()) {
    return {};
  }
  auto *result_reg = static_cast<TargetVirtualReg *>(result);
  std::string key = std::to_string(static_cast<int>(opcode)) + "/" +
                    std::to_string(static_cast<int>(result_reg->GetRegSize())) + "." +
                    std::to_string(static_cast<int>(result_reg->GetRegClass()));
  std::vector<std::string> operands;
  for (auto *operand : instruction->GetOperands()) {
    auto operand_key = OperandKey(operand);
    if (operand_key.empty()) {
      return {};
    }
    operands.push_back(std::move(operand_key));
  }
  if (info.HasFlag(RVInstFlag::Commutable) && operands.size() == 2 && operands[1] < operands[0]) {
    std::swap(operands[0], operands[1]);
  }
  for (const auto &operand : operands) {
    key += ":" + operand;
  }
  return key;
}

auto RISCVLocalCSE::RunOnFunction(TargetFunction *function) -> bool {
  MachineCFG cfg(function);
  MachineLiveness liveness(function, cfg);
  std::unordered_map<TargetOperand *, TargetBlock *> definitions;
  std::unordered_map<TargetBlock *, std::unordered_map<TargetOperand *, size_t>> last_local_use;
  for (auto *block : function->GetBlocks()) {
    const auto &instructions = block->GetInsts();
    for (size_t index = 0; index < instructions.size(); ++index) {
      auto *instruction = instructions[index];
      auto *result = instruction->GetResult();
      if (result != nullptr && result->IsVirtualReg()) {
        definitions[result] = block;
      }
      for (auto *operand : instruction->GetOperands()) {
        if (operand->IsVirtualReg()) {
          last_local_use[block][operand] = index;
        }
      }
    }
  }
  std::unordered_map<TargetOperand *, TargetOperand *> replacements;
  std::unordered_set<TargetInst *> redundant;
  std::unordered_set<TargetBlock *> visited;

  struct AvailableValue {
    TargetOperand *value_;
    TargetBlock *block_;
    size_t instruction_index_;
  };
  auto safe_across_iterations = [&](TargetBlock *canonical_block, TargetBlock *use_block,
                                    TargetInst *instruction) -> bool {
    for (const auto &loop : cfg.NaturalLoops()) {
      if (loop.blocks_.count(use_block) == 0) {
        continue;
      }
      if (loop.blocks_.count(canonical_block) != 0) {
        // The canonical computation must execute on every cycle before the
        // duplicate. Dominating all in-loop predecessors of the header proves
        // that no backedge can reuse a value from an older iteration.
        for (auto *predecessor : cfg.Predecessors(loop.header_)) {
          if (loop.blocks_.count(predecessor) != 0 && !cfg.Dominates(canonical_block, predecessor)) {
            return false;
          }
        }
      } else {
        // A value computed outside the loop is reusable only when every
        // register operand is loop invariant.
        for (auto *operand : instruction->GetOperands()) {
          if (operand->IsVirtualReg() && definitions.count(operand) != 0 &&
              loop.blocks_.count(definitions[operand]) != 0) {
            return false;
          }
        }
      }
    }
    return true;
  };
  using Available = std::unordered_map<std::string, AvailableValue>;
  auto visit = [&](auto &&self, TargetBlock *block, Available available) -> void {
    if (!visited.insert(block).second) {
      return;
    }
    // Cheap constants/addresses are intentionally scoped to one block. Reusing
    // them through the dominator tree turns a one-instruction rematerialization
    // into a function-long live range, which raises pressure and spill cost.
    Available local_rematerializable;
    const auto &instructions = block->GetInsts();
    for (size_t instruction_index = 0; instruction_index < instructions.size(); ++instruction_index) {
      auto *instruction = instructions[instruction_index];
      std::vector<TargetOperand *> operands = instruction->GetOperands();
      for (auto *&operand : operands) {
        operand = Resolve(operand, replacements);
      }
      instruction->SetOperands(std::move(operands));

      auto key = KeyOf(instruction);
      if (key.empty()) {
        continue;
      }
      auto opcode = static_cast<RISCVInst *>(instruction)->GetOpcode();
      bool rematerializable = GetRVOpcodeInfo(opcode).HasFlag(RVInstFlag::Rematerializable);
      auto &scope = rematerializable ? local_rematerializable : available;
      auto found = scope.find(key);
      // Cross-block CSE is profitable only when the canonical value is already
      // live into this block. Otherwise deleting a cheap recomputation extends
      // the canonical live range through intervening CFG and may create spills.
      bool local_profitable = false;
      if (found != scope.end() && found->second.block_ == block) {
        size_t existing_end = found->second.instruction_index_;
        auto last_use = last_local_use[block].find(found->second.value_);
        if (last_use != last_local_use[block].end()) {
          existing_end = std::max(existing_end, last_use->second);
        }
        local_profitable = rematerializable || instruction_index <= existing_end;
      }
      bool no_live_range_extension =
          found != scope.end() &&
          (local_profitable ||
           (found->second.block_ != block && liveness.LiveIn(block).count(found->second.value_) != 0 &&
            safe_across_iterations(found->second.block_, block, instruction)));
      if (no_live_range_extension) {
        replacements[instruction->GetResult()] = found->second.value_;
        redundant.insert(instruction);
      } else {
        scope[key] = {instruction->GetResult(), block, instruction_index};
      }
    }
    for (auto *child : cfg.DominatorChildren(block)) {
      self(self, child, available);
    }
  };

  // Entry first, then any unreachable root with a fresh availability scope.
  if (!function->GetBlocks().empty()) {
    visit(visit, function->GetBlocks().front(), {});
  }
  for (auto *block : function->GetBlocks()) {
    if (visited.count(block) == 0) {
      visit(visit, block, {});
    }
  }
  if (replacements.empty()) {
    return false;
  }

  for (auto *block : function->GetBlocks()) {
    std::vector<TargetInst *> kept;
    for (auto *instruction : block->GetInsts()) {
      if (redundant.count(instruction) != 0) {
        continue;
      }
      std::vector<TargetOperand *> operands = instruction->GetOperands();
      for (auto *&operand : operands) {
        operand = Resolve(operand, replacements);
      }
      instruction->SetOperands(std::move(operands));
      kept.push_back(instruction);
    }
    block->SetInsts(std::move(kept));
  }
  for (const auto &[edge, instructions] : function->GetAllEdgeCode()) {
    (void)edge;
    for (auto *instruction : instructions) {
      std::vector<TargetOperand *> operands = instruction->GetOperands();
      for (auto *&operand : operands) {
        operand = Resolve(operand, replacements);
      }
      instruction->SetOperands(std::move(operands));
    }
  }
  return true;
}

auto RISCVPostPhiLocalCSE::RunOnFunction(TargetFunction *function) -> bool {
  std::unordered_map<size_t, int> definition_count;
  for (auto *block : function->GetBlocks()) {
    for (auto *instruction : block->GetInsts()) {
      auto *result = instruction->GetResult();
      if (result != nullptr && result->IsVirtualReg()) {
        definition_count[static_cast<TargetVirtualReg *>(result)->GetID()]++;
      }
    }
  }
  auto single_definition = [&](TargetOperand *operand) {
    return operand != nullptr && operand->IsVirtualReg() &&
           definition_count[static_cast<TargetVirtualReg *>(operand)->GetID()] == 1;
  };

  std::unordered_map<TargetOperand *, TargetOperand *> replacements;
  for (auto *block : function->GetBlocks()) {
    std::unordered_map<std::string, TargetOperand *> available;
    std::unordered_map<size_t, std::vector<std::string>> readers;
    for (auto *instruction : block->GetInsts()) {
      auto *result = instruction->GetResult();
      if (result != nullptr && result->IsVirtualReg()) {
        size_t id = static_cast<TargetVirtualReg *>(result)->GetID();
        auto reader = readers.find(id);
        if (reader != readers.end()) {
          for (const auto &key : reader->second) {
            available.erase(key);
          }
          readers.erase(reader);
        }
      }
      auto key = RISCVLocalCSE::KeyOf(instruction);
      if (key.empty() || !single_definition(result)) {
        continue;
      }
      auto found = available.find(key);
      if (found != available.end()) {
        replacements[result] = found->second;
        continue;
      }
      available[key] = result;
      for (auto *operand : instruction->GetOperands()) {
        if (operand->IsVirtualReg()) {
          readers[static_cast<TargetVirtualReg *>(operand)->GetID()].push_back(key);
        }
      }
    }
  }
  if (replacements.empty()) {
    return false;
  }

  for (auto *block : function->GetBlocks()) {
    std::vector<TargetInst *> kept;
    for (auto *instruction : block->GetInsts()) {
      if (replacements.count(instruction->GetResult()) != 0) {
        continue;
      }
      std::vector<TargetOperand *> operands = instruction->GetOperands();
      for (auto *&operand : operands) {
        auto replacement = replacements.find(operand);
        if (replacement != replacements.end()) {
          operand = replacement->second;
        }
      }
      instruction->SetOperands(std::move(operands));
      kept.push_back(instruction);
    }
    block->SetInsts(std::move(kept));
  }
  return true;
}

}  // namespace backend::targetir::riscv
