#include "backend/targets/riscv/pass/postra.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#include "backend/targets/riscv/inst/riscvinst.h"
#include "backend/targets/riscv/regs/riscvreg.h"
#include "backend/targets/utils/context/targetcontext.h"
#include "backend/targets/utils/operand/targetliteral.h"
#include "backend/targets/utils/operand/targetreg.h"
#include "backend/targets/utils/operand/targetsymbol.h"
#include "backend/targets/utils/struct/targetblock.h"
#include "backend/targets/utils/struct/targetfunction.h"

namespace backend::targetir::riscv {

namespace {

enum class SpillKind : uint8_t { GPRWord, GPRDouble, FPRWord };

struct SpillValue {
  SpillKind kind_;
  std::optional<int> available_reg_;
  std::optional<size_t> last_store_;
  bool memory_read_since_store_ = false;
};

auto PhysicalRegKey(TargetOperand *operand) -> std::optional<int> {
  if (operand == nullptr || !operand->IsPhysicalReg()) {
    return std::nullopt;
  }
  auto *reg = static_cast<TargetPhysicalReg *>(operand);
  return (static_cast<int>(reg->GetRegClass()) << 8) | reg->GetID();
}

auto IsZeroReg(TargetOperand *operand) -> bool {
  auto key = PhysicalRegKey(operand);
  return key.has_value() && *key == static_cast<int>(GPR::Zero);
}

auto IsImmediate(TargetOperand *operand, int64_t value) -> bool {
  return operand != nullptr && operand->GetOpType() == TargetOperandType::Immediate &&
         static_cast<TargetIntLiteral *>(operand)->GetValue() == value;
}

auto LoadKind(RVOpcode opcode) -> std::optional<SpillKind> {
  switch (opcode) {
    case RVOpcode::LW:
      return SpillKind::GPRWord;
    case RVOpcode::LD:
      return SpillKind::GPRDouble;
    case RVOpcode::FLW:
      return SpillKind::FPRWord;
    default:
      return std::nullopt;
  }
}

auto StoreKind(RVOpcode opcode) -> std::optional<SpillKind> {
  switch (opcode) {
    case RVOpcode::SW:
      return SpillKind::GPRWord;
    case RVOpcode::SD:
      return SpillKind::GPRDouble;
    case RVOpcode::FSW:
      return SpillKind::FPRWord;
    default:
      return std::nullopt;
  }
}

auto SpillSlot(TargetInst *inst, size_t spill_base, size_t spill_slots, bool is_store) -> std::optional<size_t> {
  const auto &ops = inst->GetOperands();
  size_t base_index = is_store ? 1 : 0;
  size_t offset_index = is_store ? 2 : 1;
  if (ops.size() <= offset_index || !ops[base_index]->IsPhysicalReg() ||
      ops[offset_index]->GetOpType() != TargetOperandType::Immediate) {
    return std::nullopt;
  }
  auto *base = static_cast<TargetPhysicalReg *>(ops[base_index]);
  if (base->GetRegClass() != TargetRegClass::General || base->GetID() != static_cast<uint8_t>(GPR::Sp)) {
    return std::nullopt;
  }
  int64_t offset = static_cast<TargetIntLiteral *>(ops[offset_index])->GetValue();
  int64_t first = static_cast<int64_t>(spill_base);
  int64_t end = first + static_cast<int64_t>(spill_slots) * 8;
  if (offset < first || offset >= end || ((offset - first) % 8) != 0) {
    return std::nullopt;
  }
  return static_cast<size_t>((offset - first) / 8);
}

auto InvalidateRegister(std::unordered_map<size_t, SpillValue> *slots, int reg) -> void {
  for (auto &[slot, value] : *slots) {
    (void)slot;
    if (value.available_reg_.has_value() && *value.available_reg_ == reg) {
      value.available_reg_.reset();
    }
  }
}

auto CleanupSpills(TargetContext *ctx, TargetBlock *block, size_t spill_base, size_t spill_slots) -> bool {
  if (spill_slots == 0) {
    return false;
  }

  bool changed = false;
  std::unordered_map<size_t, SpillValue> slots;
  std::vector<TargetInst *> rewritten;
  rewritten.reserve(block->GetInsts().size());

  for (auto *inst : block->GetInsts()) {
    auto opcode = static_cast<RISCVInst *>(inst)->GetOpcode();
    auto load_kind = LoadKind(opcode);
    auto store_kind = StoreKind(opcode);

    if (load_kind.has_value()) {
      auto slot = SpillSlot(inst, spill_base, spill_slots, false);
      auto dst = PhysicalRegKey(inst->GetResult());
      if (slot.has_value() && dst.has_value()) {
        auto found = slots.find(*slot);
        if (found != slots.end() && found->second.kind_ == *load_kind && found->second.available_reg_.has_value()) {
          int source = *found->second.available_reg_;
          if (source == *dst) {
            changed = true;
            continue;
          }

          InvalidateRegister(&slots, *dst);
          auto move_opcode = *load_kind == SpillKind::FPRWord ? RVOpcode::FMV_S : RVOpcode::MV;
          auto *move = ctx->CreateInst<RISCVInst>(move_opcode);
          move->SetResult(inst->GetResult());
          auto *source_operand = ctx->PhysReg(
              *load_kind == SpillKind::FPRWord ? TargetRegClass::Float : TargetRegClass::General,
              inst->GetResult()->IsPhysicalReg() ? static_cast<TargetPhysicalReg *>(inst->GetResult())->GetRegSize()
                                                 : TargetRegSize::QWord,
              static_cast<uint8_t>(source & 0xff));
          move->AddOperand(source_operand);
          rewritten.push_back(move);
          changed = true;
          continue;
        }

        InvalidateRegister(&slots, *dst);
        auto [it, inserted] = slots.emplace(*slot, SpillValue{*load_kind, *dst, std::nullopt, false});
        if (!inserted) {
          if (it->second.last_store_.has_value()) {
            it->second.memory_read_since_store_ = true;
          }
          it->second.kind_ = *load_kind;
          it->second.available_reg_ = *dst;
        }
        rewritten.push_back(inst);
        continue;
      }
    }

    if (store_kind.has_value()) {
      auto slot = SpillSlot(inst, spill_base, spill_slots, true);
      const auto &ops = inst->GetOperands();
      auto source = ops.empty() ? std::optional<int>{} : PhysicalRegKey(ops[0]);
      if (slot.has_value() && source.has_value()) {
        auto found = slots.find(*slot);
        if (found != slots.end() && found->second.kind_ == *store_kind && found->second.available_reg_.has_value() &&
            *found->second.available_reg_ == *source) {
          changed = true;
          continue;
        }

        if (found != slots.end() && found->second.last_store_.has_value() && !found->second.memory_read_since_store_) {
          rewritten[*found->second.last_store_] = nullptr;
          changed = true;
        }
        size_t store_index = rewritten.size();
        slots[*slot] = SpillValue{*store_kind, *source, store_index, false};
        rewritten.push_back(inst);
        continue;
      }
    }

    if (opcode == RVOpcode::CALL) {
      for (auto &[slot, value] : slots) {
        (void)slot;
        value.available_reg_.reset();
      }
    } else if (auto result = PhysicalRegKey(inst->GetResult()); result.has_value() && !IsZeroReg(inst->GetResult())) {
      InvalidateRegister(&slots, *result);
    }
    rewritten.push_back(inst);
  }

  if (!changed) {
    return false;
  }
  std::vector<TargetInst *> kept;
  kept.reserve(rewritten.size());
  for (auto *inst : rewritten) {
    if (inst != nullptr) {
      kept.push_back(inst);
    }
  }
  block->SetInsts(std::move(kept));
  return true;
}

auto CleanupCopies(TargetBlock *block) -> bool {
  bool changed = false;
  int next_value = 1;
  std::unordered_map<int, int> values;
  auto value_of = [&](int reg) -> int {
    auto [it, inserted] = values.emplace(reg, next_value);
    if (inserted) {
      ++next_value;
    }
    return it->second;
  };

  std::vector<TargetInst *> kept;
  kept.reserve(block->GetInsts().size());
  for (auto *inst : block->GetInsts()) {
    auto opcode = static_cast<RISCVInst *>(inst)->GetOpcode();
    bool is_move = opcode == RVOpcode::MV || opcode == RVOpcode::FMV_S;
    auto dst = PhysicalRegKey(inst->GetResult());
    const auto &ops = inst->GetOperands();
    auto src = is_move && !ops.empty() ? PhysicalRegKey(ops[0]) : std::optional<int>{};

    if (is_move && dst.has_value() && src.has_value() && !IsZeroReg(inst->GetResult())) {
      int source_value = value_of(*src);
      if (value_of(*dst) == source_value) {
        changed = true;
        continue;
      }
      values[*dst] = source_value;
      kept.push_back(inst);
      continue;
    }

    if (opcode == RVOpcode::CALL) {
      values.clear();
    } else if (dst.has_value() && !IsZeroReg(inst->GetResult())) {
      values[*dst] = next_value++;
    }
    kept.push_back(inst);
  }

  if (changed) {
    block->SetInsts(std::move(kept));
  }
  return changed;
}

// RV64 signed i32 division by two uses the truncation bias
//   (x + ((x >> 31) >>> 31)) >> 1.
// After register allocation, collapse the adjacent two-shift sign-bit
// extraction to `srliw ..., x, 31`.  Doing this post-RA deliberately leaves
// the interference graph and coloring unchanged while saving one instruction
// in every executed sequence.
auto FoldSignBitExtraction(TargetBlock *block) -> bool {
  const auto &insts = block->GetInsts();
  if (insts.size() < 2) {
    return false;
  }

  bool changed = false;
  std::vector<TargetInst *> kept;
  kept.reserve(insts.size());
  for (size_t i = 0; i < insts.size(); ++i) {
    auto *first = insts[i];
    if (i + 1 < insts.size() && static_cast<RISCVInst *>(first)->GetOpcode() == RVOpcode::SRAIW) {
      auto *second = insts[i + 1];
      const auto &first_ops = first->GetOperands();
      const auto &second_ops = second->GetOperands();
      auto sign = PhysicalRegKey(first->GetResult());
      auto sign_use = second_ops.empty() ? std::optional<int>{} : PhysicalRegKey(second_ops[0]);
      auto second_result = PhysicalRegKey(second->GetResult());
      if (static_cast<RISCVInst *>(second)->GetOpcode() == RVOpcode::SRLIW && first_ops.size() == 2 &&
          second_ops.size() == 2 && IsImmediate(first_ops[1], 31) && IsImmediate(second_ops[1], 31) &&
          sign.has_value() && sign_use == sign && second_result == sign) {
        second->SetOperands({first_ops[0], second_ops[1]});
        kept.push_back(second);
        ++i;
        changed = true;
        continue;
      }
    }
    kept.push_back(first);
  }

  if (changed) {
    block->SetInsts(std::move(kept));
  }
  return changed;
}

auto FoldShortCircuitMerges(TargetFunction *func) -> bool {
  std::unordered_map<std::string, TargetBlock *> by_label;
  for (auto *block : func->GetBlocks()) {
    by_label[block->GetLabel()] = block;
  }

  auto target_block = [&](TargetOperand *operand) -> TargetBlock * {
    if (operand == nullptr || operand->GetOpType() != TargetOperandType::Label) {
      return nullptr;
    }
    auto found = by_label.find(static_cast<TargetLabel *>(operand)->GetName());
    return found == by_label.end() ? nullptr : found->second;
  };
  auto resolve_forwarder = [&](TargetOperand *operand) -> TargetBlock * {
    auto *block = target_block(operand);
    std::vector<TargetBlock *> seen;
    while (block != nullptr) {
      if (std::find(seen.begin(), seen.end(), block) != seen.end()) {
        return nullptr;
      }
      seen.push_back(block);
      const auto &insts = block->GetInsts();
      if (insts.size() != 1) {
        return block;
      }
      auto *jump = static_cast<RISCVInst *>(insts.front());
      const auto &ops = jump->GetOperands();
      if (jump->GetOpcode() != RVOpcode::J || ops.size() != 1) {
        return block;
      }
      block = target_block(ops[0]);
    }
    return nullptr;
  };
  auto equality_branch = [](RVOpcode opcode) { return opcode == RVOpcode::BEQ || opcode == RVOpcode::BNE; };
  auto compares_reg_to_zero = [](TargetInst *inst, int reg) {
    const auto &ops = inst->GetOperands();
    if (ops.size() != 3) {
      return false;
    }
    auto lhs = PhysicalRegKey(ops[0]);
    auto rhs = PhysicalRegKey(ops[1]);
    return (lhs.has_value() && *lhs == reg && IsZeroReg(ops[1])) ||
           (rhs.has_value() && *rhs == reg && IsZeroReg(ops[0]));
  };
  auto defines_before_jump = [](TargetBlock *block, int reg) {
    const auto &insts = block->GetInsts();
    if (insts.empty() || static_cast<RISCVInst *>(insts.back())->GetOpcode() != RVOpcode::J ||
        insts.back()->GetOperands().size() != 1 ||
        insts.back()->GetOperands()[0]->GetOpType() != TargetOperandType::Label) {
      return false;
    }
    bool defined = false;
    for (size_t i = 0; i + 1 < insts.size(); ++i) {
      for (auto *operand : insts[i]->GetOperands()) {
        auto used = PhysicalRegKey(operand);
        if (used.has_value() && *used == reg && !defined) {
          return false;
        }
      }
      auto result = PhysicalRegKey(insts[i]->GetResult());
      if (result.has_value() && *result == reg) {
        defined = true;
      }
    }
    return defined;
  };

  bool changed = false;
  for (auto *block : func->GetBlocks()) {
    const auto &insts = block->GetInsts();
    if (insts.size() < 3) {
      continue;
    }
    auto *materialize = static_cast<RISCVInst *>(insts[insts.size() - 3]);
    auto *branch = static_cast<RISCVInst *>(insts[insts.size() - 2]);
    auto *jump = static_cast<RISCVInst *>(insts.back());
    auto materialize_opcode = materialize->GetOpcode();
    auto branch_opcode = branch->GetOpcode();
    const auto &materialize_ops = materialize->GetOperands();
    auto branch_ops = branch->GetOperands();
    auto jump_ops = jump->GetOperands();
    auto bool_reg = PhysicalRegKey(materialize->GetResult());
    auto cond_reg = materialize_ops.empty() ? std::optional<int>{} : PhysicalRegKey(materialize_ops[0]);
    if (!materialize->IsShortCircuitMaterialization() ||
        (materialize_opcode != RVOpcode::SEQZ && materialize_opcode != RVOpcode::SNEZ) ||
        !equality_branch(branch_opcode) || jump->GetOpcode() != RVOpcode::J || materialize_ops.size() != 1 ||
        branch_ops.size() != 3 || jump_ops.size() != 1 || !bool_reg.has_value() || !cond_reg.has_value() ||
        *bool_reg == *cond_reg || !compares_reg_to_zero(branch, *cond_reg)) {
      continue;
    }

    auto *branch_edge = resolve_forwarder(branch_ops[2]);
    auto *jump_edge = resolve_forwarder(jump_ops[0]);
    TargetBlock *merge = nullptr;
    bool short_is_branch = false;
    auto is_merge = [&](TargetBlock *candidate) {
      if (candidate == nullptr || candidate->GetInsts().size() != 2) {
        return false;
      }
      auto *merge_branch = static_cast<RISCVInst *>(candidate->GetInsts()[0]);
      auto *merge_jump = static_cast<RISCVInst *>(candidate->GetInsts()[1]);
      return equality_branch(merge_branch->GetOpcode()) && compares_reg_to_zero(merge_branch, *bool_reg) &&
             merge_jump->GetOpcode() == RVOpcode::J && merge_jump->GetOperands().size() == 1;
    };
    if (is_merge(branch_edge) && jump_edge != nullptr && defines_before_jump(jump_edge, *bool_reg) &&
        resolve_forwarder(jump_edge->GetInsts().back()->GetOperands()[0]) == branch_edge) {
      merge = branch_edge;
      short_is_branch = true;
    } else if (is_merge(jump_edge) && branch_edge != nullptr && defines_before_jump(branch_edge, *bool_reg) &&
               resolve_forwarder(branch_edge->GetInsts().back()->GetOperands()[0]) == jump_edge) {
      merge = jump_edge;
      short_is_branch = false;
    }
    if (merge == nullptr) {
      continue;
    }

    bool condition_is_zero = short_is_branch ? branch_opcode == RVOpcode::BEQ : branch_opcode == RVOpcode::BNE;
    bool known_boolean = materialize_opcode == RVOpcode::SEQZ ? condition_is_zero : !condition_is_zero;
    auto *merge_branch = static_cast<RISCVInst *>(merge->GetInsts()[0]);
    auto *merge_jump = static_cast<RISCVInst *>(merge->GetInsts()[1]);
    bool merge_branch_taken = merge_branch->GetOpcode() == RVOpcode::BEQ ? !known_boolean : known_boolean;
    TargetOperand *known_target = merge_branch_taken ? merge_branch->GetOperands()[2] : merge_jump->GetOperands()[0];
    if (short_is_branch) {
      branch->SetOperands({branch_ops[0], branch_ops[1], known_target});
    } else {
      jump->SetOperands({known_target});
    }
    std::vector<TargetInst *> kept(insts.begin(), insts.end() - 3);
    kept.push_back(branch);
    kept.push_back(jump);
    block->SetInsts(std::move(kept));
    changed = true;
  }
  return changed;
}

}  // namespace

auto RISCVPostRA::RunOnFunction(TargetFunction *func, size_t spill_base, size_t spill_slots) -> bool {
  bool changed = false;
  for (auto *block : func->GetBlocks()) {
    changed |= CleanupSpills(ctx_, block, spill_base, spill_slots);
    changed |= FoldSignBitExtraction(block);
    changed |= CleanupCopies(block);
  }
  changed |= FoldShortCircuitMerges(func);
  return changed;
}

}  // namespace backend::targetir::riscv
