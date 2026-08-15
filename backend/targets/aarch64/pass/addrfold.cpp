#include "backend/targets/aarch64/pass/addrfold.h"
#include "backend/targets/aarch64/inst/aarch64inst.h"
#include "backend/targets/utils/context/targetcontext.h"
#include "backend/targets/utils/operand/targetliteral.h"
#include "backend/targets/utils/operand/targetreg.h"
#include "backend/targets/utils/struct/targetblock.h"
#include "backend/targets/utils/struct/targetfunction.h"

namespace backend::targetir::aarch64 {

namespace {
// AArch64 unsigned-offset ldr/str immediate: byte offset must be a multiple of the
// access size and fit in 12 bits (i.e. [0, 4095 * size]).
constexpr int UOFF_IMM_MAX = 4095;

auto RegSizeOf(TargetOperand *op) -> TargetRegSize {
  if (op->IsVirtualReg()) {
    return static_cast<TargetVirtualReg *>(op)->GetRegSize();
  }
  if (op->IsPhysicalReg()) {
    return static_cast<TargetPhysicalReg *>(op)->GetRegSize();
  }
  return TargetRegSize::QWord;
}

auto IsQWord(TargetOperand *op) -> bool { return RegSizeOf(op) == TargetRegSize::QWord; }

// Access width in bytes for the loaded/stored value register.
auto AccessBytes(TargetRegSize size) -> int {
  if (size == TargetRegSize::OWord) {
    return 16;
  }
  return size == TargetRegSize::QWord ? 8 : 4;
}

auto SamePhysicalStorage(TargetOperand *lhs, TargetOperand *rhs) -> bool {
  if (lhs == nullptr || rhs == nullptr || !lhs->IsPhysicalReg() || !rhs->IsPhysicalReg()) {
    return false;
  }
  auto *lhs_reg = static_cast<TargetPhysicalReg *>(lhs);
  auto *rhs_reg = static_cast<TargetPhysicalReg *>(rhs);
  return lhs_reg->GetRegClass() == rhs_reg->GetRegClass() && lhs_reg->GetID() == rhs_reg->GetID();
}

auto UsesPhysicalStorage(TargetInst *inst, TargetOperand *reg) -> bool {
  for (auto *operand : inst->GetOperands()) {
    if (SamePhysicalStorage(operand, reg)) {
      return true;
    }
  }
  return false;
}
}  // namespace

auto AArch64AddrFold::BuildDefs(TargetFunction *func) -> void {
  def_.clear();
  def_count_.clear();
  for (auto *block : func->GetBlocks()) {
    for (auto *inst : block->GetInsts()) {
      auto *res = inst->GetResult();
      if (res != nullptr && res->IsVirtualReg()) {
        def_count_[res]++;
        def_[res] = inst;
      }
    }
  }
}

auto AArch64AddrFold::SingleDef(TargetOperand *op) const -> TargetInst * {
  if (op == nullptr || !op->IsVirtualReg()) {
    return nullptr;
  }
  auto cit = def_count_.find(op);
  if (cit == def_count_.end() || cit->second != 1) {
    return nullptr;
  }
  auto it = def_.find(op);
  return it == def_.end() ? nullptr : it->second;
}

auto AArch64AddrFold::RunOnFunction(TargetFunction *func) -> bool {
  BuildDefs(func);
  bool changed = false;
  for (auto *block : func->GetBlocks()) {
    for (auto *inst : block->GetInsts()) {
      auto *ai = static_cast<AArch64Inst *>(inst);
      auto opc = ai->GetOpcode();
      const bool is_reg_load = opc == A64Opcode::LDR_REG;
      const bool is_reg_store = opc == A64Opcode::STR_REG;
      if (is_reg_load || is_reg_store) {
        const auto &ops = inst->GetOperands();
        auto *value = is_reg_load ? inst->GetResult() : ops[0];
        auto *base = is_reg_load ? ops[0] : ops[1];
        auto *index = is_reg_load ? ops[1] : ops[2];
        auto *shift_op = is_reg_load ? ops[2] : ops[3];
        int shift = static_cast<TargetIntLiteral *>(shift_op)->GetValue();

        // Resolve the common `movz wi, #C; sxtw xi, wi` index chain left by
        // lowering. Turning it into an unsigned byte offset both removes address
        // setup and exposes adjacent memory operations to SLP vectorization.
        auto *extend = SingleDef(index);
        TargetOperand *narrow = nullptr;
        if (extend != nullptr && static_cast<AArch64Inst *>(extend)->GetOpcode() == A64Opcode::SXTW) {
          narrow = extend->GetOperands()[0];
        }
        auto *constant = SingleDef(narrow);
        if (constant != nullptr && static_cast<AArch64Inst *>(constant)->GetOpcode() == A64Opcode::MOVZ) {
          int index_value = static_cast<TargetIntLiteral *>(constant->GetOperands()[0])->GetValue();
          int offset = index_value << shift;
          int width = AccessBytes(RegSizeOf(value));
          if (offset >= 0 && offset % width == 0 && offset <= UOFF_IMM_MAX * width) {
            ai->SetOpcode(is_reg_load ? A64Opcode::LDR_OFF : A64Opcode::STR_OFF);
            if (is_reg_load) {
              inst->SetOperands({base, ctx_->Imm(offset)});
            } else {
              inst->SetOperands({value, base, ctx_->Imm(offset)});
            }
            changed = true;
          }
        }
        continue;
      }
      const bool is_load = opc == A64Opcode::LDR_OFF;
      const bool is_store = opc == A64Opcode::STR_OFF;
      if (!is_load && !is_store) {
        continue;
      }
      const auto &ops = inst->GetOperands();
      // load:  {base, #off}  (result = value)   store: {value, base, #off}
      auto *base = is_load ? ops[0] : ops[1];
      auto *off = is_load ? ops[1] : ops[2];
      if (off->GetOpType() != TargetOperandType::Immediate || static_cast<TargetIntLiteral *>(off)->GetValue() != 0) {
        continue;  // already carries a folded offset / register form
      }
      auto *value = is_load ? inst->GetResult() : ops[0];
      int width = AccessBytes(RegSizeOf(value));

      auto *def = SingleDef(base);
      if (def == nullptr) {
        continue;
      }
      auto *dai = static_cast<AArch64Inst *>(def);
      const auto &dops = def->GetOperands();

      if (dai->GetOpcode() == A64Opcode::ADD_I) {
        // base = b + #imm  ->  [b, #imm]
        auto *b = dops[0];
        int imm = static_cast<TargetIntLiteral *>(dops[1])->GetValue();
        if (imm % width != 0 || imm < 0 || imm > UOFF_IMM_MAX * width) {
          continue;
        }
        if (is_load) {
          inst->SetOperands({b, ctx_->Imm(imm)});
        } else {
          inst->SetOperands({value, b, ctx_->Imm(imm)});
        }
        changed = true;
        continue;
      }

      if (dai->GetOpcode() == A64Opcode::ADD) {
        // base = b + r  ->  [b, r, lsl #s]. Prefer combining a feeding shift.
        auto *b = dops[0];
        auto *r = dops[1];
        TargetOperand *index = r;
        int shift = 0;
        auto *rdef = SingleDef(r);
        if (rdef != nullptr && static_cast<AArch64Inst *>(rdef)->GetOpcode() == A64Opcode::LSL_I) {
          const auto &sops = rdef->GetOperands();
          int s = static_cast<TargetIntLiteral *>(sops[1])->GetValue();
          // The register-offset form scales only by log2(access size); a shift by
          // exactly that amount folds, otherwise fall back to the pre-shift add.
          if (s == __builtin_ctz(static_cast<unsigned>(width)) && IsQWord(sops[0])) {
            index = sops[0];
            shift = s;
          }
        }
        if (!IsQWord(index)) {
          continue;  // a 32-bit index would need an sxtw/uxtw extend form
        }
        if (is_load) {
          ai->SetOpcode(A64Opcode::LDR_REG);
          inst->SetOperands({b, index, ctx_->Imm(shift)});
        } else {
          ai->SetOpcode(A64Opcode::STR_REG);
          inst->SetOperands({value, b, index, ctx_->Imm(shift)});
        }
        changed = true;
        continue;
      }
    }
  }
  return changed;
}

auto AArch64PostRAAddrFold::RunOnFunction(TargetFunction *func) -> bool {
  bool changed = false;
  for (auto *block : func->GetBlocks()) {
    const auto &instructions = block->GetInsts();
    std::vector<TargetInst *> kept;
    kept.reserve(instructions.size());
    for (size_t index = 0; index < instructions.size(); ++index) {
      auto *extend = static_cast<AArch64Inst *>(instructions[index]);
      if (extend->GetOpcode() != A64Opcode::SXTW || index + 1 >= instructions.size() ||
          extend->GetResult() == nullptr || extend->GetOperands().size() != 1 ||
          !extend->GetResult()->IsPhysicalReg() || !extend->GetOperands()[0]->IsPhysicalReg()) {
        kept.emplace_back(extend);
        continue;
      }

      auto *memory = static_cast<AArch64Inst *>(instructions[index + 1]);
      bool is_load = memory->GetOpcode() == A64Opcode::LDR_REG;
      bool is_store = memory->GetOpcode() == A64Opcode::STR_REG;
      if (!is_load && !is_store) {
        kept.emplace_back(extend);
        continue;
      }
      const auto &memory_operands = memory->GetOperands();
      size_t memory_index = is_load ? 1 : 2;
      if (memory_operands.size() <= memory_index ||
          !SamePhysicalStorage(memory_operands[memory_index], extend->GetResult())) {
        kept.emplace_back(extend);
        continue;
      }

      // The extended temporary must die here. A result definition by the load is an immediate
      // kill; otherwise the first later mention in this block must be a definition, not a use.
      bool dies_here = SamePhysicalStorage(memory->GetResult(), extend->GetResult());
      if (!dies_here) {
        for (size_t later = index + 2; later < instructions.size(); ++later) {
          auto *candidate = instructions[later];
          if (UsesPhysicalStorage(candidate, extend->GetResult())) {
            break;
          }
          if (SamePhysicalStorage(candidate->GetResult(), extend->GetResult())) {
            dies_here = true;
            break;
          }
        }
      }
      if (!dies_here) {
        kept.emplace_back(extend);
        continue;
      }

      std::vector<TargetOperand *> operands = memory_operands;
      operands[memory_index] = extend->GetOperands()[0];
      memory->SetOperands(operands);
      memory->SetOpcode(is_load ? A64Opcode::LDR_SXTW : A64Opcode::STR_SXTW);
      changed = true;
      ++folded_count_;
      // Do not keep the now-dead SXTW. The memory operation is visited and retained on the next
      // iteration, preserving instruction order without changing any other allocated register.
    }
    block->SetInsts(std::move(kept));
  }
  return changed;
}

}  // namespace backend::targetir::aarch64
