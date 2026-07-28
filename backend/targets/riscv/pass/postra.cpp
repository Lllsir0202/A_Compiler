#include "backend/targets/riscv/pass/postra.h"

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#include "backend/targets/riscv/inst/riscvinst.h"
#include "backend/targets/riscv/regs/riscvreg.h"
#include "backend/targets/utils/context/targetcontext.h"
#include "backend/targets/utils/operand/targetliteral.h"
#include "backend/targets/utils/operand/targetreg.h"
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

}  // namespace

auto RISCVPostRA::RunOnFunction(TargetFunction *func, size_t spill_base, size_t spill_slots) -> bool {
  bool changed = false;
  for (auto *block : func->GetBlocks()) {
    changed |= CleanupSpills(ctx_, block, spill_base, spill_slots);
    changed |= FoldSignBitExtraction(block);
    changed |= CleanupCopies(block);
  }
  return changed;
}

}  // namespace backend::targetir::riscv
