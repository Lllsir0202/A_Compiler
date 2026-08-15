#include "backend/targets/aarch64/pass/localcse.h"
#include <string>
#include <unordered_map>
#include <vector>
#include "backend/targets/aarch64/inst/aarch64inst.h"
#include "backend/targets/utils/operand/targetliteral.h"
#include "backend/targets/utils/operand/targetreg.h"
#include "backend/targets/utils/operand/targetsymbol.h"
#include "backend/targets/utils/struct/targetblock.h"
#include "backend/targets/utils/struct/targetfunction.h"

namespace backend::targetir::aarch64 {

namespace {
// Pure register-to-register opcodes: a deterministic function of their operands
// with no memory or flag effect. Loads/stores, cmp/cset, movk (reads its dest),
// branches, calls and the stp/ldp pseudos are intentionally absent.
auto IsPure(A64Opcode op) -> bool {
  switch (op) {
    case A64Opcode::ADD:
    case A64Opcode::SUB:
    case A64Opcode::MUL:
    case A64Opcode::SDIV:
    case A64Opcode::AND:
    case A64Opcode::ORR:
    case A64Opcode::EOR:
    case A64Opcode::LSLV:
    case A64Opcode::LSRV:
    case A64Opcode::ASRV:
    case A64Opcode::MSUB:
    case A64Opcode::MADD:
    case A64Opcode::ADD_I:
    case A64Opcode::SUB_I:
    case A64Opcode::AND_I:
    case A64Opcode::ORR_I:
    case A64Opcode::EOR_I:
    case A64Opcode::LSL_I:
    case A64Opcode::LSR_I:
    case A64Opcode::ASR_I:
    case A64Opcode::MOV:
    case A64Opcode::MOVZ:
    case A64Opcode::MOVN:
    case A64Opcode::SXTW:
    case A64Opcode::ADRP:
    case A64Opcode::ADD_LO12:
    case A64Opcode::FADD:
    case A64Opcode::FSUB:
    case A64Opcode::FMUL:
    case A64Opcode::FDIV:
    case A64Opcode::FMOV:
    case A64Opcode::FMOV_TO_GPR:
    case A64Opcode::FMOV_FROM_GPR:
    case A64Opcode::SCVTF:
    case A64Opcode::FCVTZS:
    case A64Opcode::SMULH:
    case A64Opcode::ADD_SHIFT_LSL:
    case A64Opcode::ADD_SHIFT_LSR:
    case A64Opcode::ADD_SHIFT_ASR:
    case A64Opcode::SUB_SHIFT_LSL:
    case A64Opcode::SUB_SHIFT_LSR:
    case A64Opcode::SUB_SHIFT_ASR:
    case A64Opcode::ADD_EXT_SXTW:
    case A64Opcode::SUB_EXT_SXTW:
    case A64Opcode::ADD_I_LSL12:
    case A64Opcode::SUB_I_LSL12:
      return true;
    default:
      return false;
  }
}
}  // namespace

auto AArch64LocalCSE::KeyOf(TargetInst *inst) -> std::string {
  auto *ai = static_cast<AArch64Inst *>(inst);
  if (!IsPure(ai->GetOpcode())) {
    return "";
  }
  auto *res = inst->GetResult();
  if (res == nullptr || !res->IsVirtualReg()) {
    return "";
  }
  // The result's width/class is not derivable from the operands (a mov/sxtw can
  // target a w- or x-view of the same source), so it must be part of the key --
  // otherwise two same-operand ops with differently-sized results merge wrongly.
  auto *rv = static_cast<TargetVirtualReg *>(res);
  std::string key = std::to_string(static_cast<int>(ai->GetOpcode()));
  key += "/" + std::to_string(static_cast<int>(ai->GetCond()));
  key += "/" + std::to_string(static_cast<int>(rv->GetRegSize())) + "." +
         std::to_string(static_cast<int>(rv->GetRegClass()));
  for (auto *op : inst->GetOperands()) {
    switch (op->GetOpType()) {
      case TargetOperandType::VirtualReg:
        key += ":v" + std::to_string(static_cast<TargetVirtualReg *>(op)->GetID());
        break;
      case TargetOperandType::Immediate:
        key += ":i" + std::to_string(static_cast<TargetIntLiteral *>(op)->GetValue());
        break;
      case TargetOperandType::Symbol:
        key += ":s" + static_cast<TargetSymbol *>(op)->GetName();
        break;
      case TargetOperandType::Label:
        key += ":l" + static_cast<TargetLabel *>(op)->GetName();
        break;
      default:
        return "";  // physical register (may be reassigned) or memory: not CSE-able
    }
  }
  return key;
}

auto AArch64LocalCSE::RunOnFunction(TargetFunction *func) -> bool {
  // Virtual registers here are NOT single-assignment: loop-carried values (phis)
  // and accumulators are rewritten, so the same vreg can be defined many times.
  // Count definitions so we only ever *replace* a single-def result (safe to RAUW
  // and delete) and only trust a single-def canonical (its value never changes).
  // The address-arithmetic temporaries (sxtw/lsl/add results) are single-def.
  std::unordered_map<size_t, int> def_count;
  for (auto *block : func->GetBlocks()) {
    for (auto *inst : block->GetInsts()) {
      auto *res = inst->GetResult();
      if (res != nullptr && res->IsVirtualReg()) {
        def_count[static_cast<TargetVirtualReg *>(res)->GetID()]++;
      }
    }
  }
  auto single_def = [&](TargetOperand *op) {
    return op != nullptr && op->IsVirtualReg() && def_count[static_cast<TargetVirtualReg *>(op)->GetID()] == 1;
  };

  // result-of-a-duplicate -> the canonical result it should become.
  std::unordered_map<TargetOperand *, TargetOperand *> replace;
  for (auto *block : func->GetBlocks()) {
    // Per-block value numbering. A cached expression stays valid only until one of
    // its (possibly multiply-defined) operands is rewritten, so when any register
    // is redefined we drop every cached expression that read it.
    std::unordered_map<std::string, TargetOperand *> seen;         // key -> canonical result
    std::unordered_map<size_t, std::vector<std::string>> readers;  // vreg id -> keys reading it
    for (auto *inst : block->GetInsts()) {
      auto *res = inst->GetResult();
      if (res != nullptr && res->IsVirtualReg()) {
        size_t rid = static_cast<TargetVirtualReg *>(res)->GetID();
        auto rit = readers.find(rid);
        if (rit != readers.end()) {
          for (const auto &k : rit->second) {
            seen.erase(k);
          }
          readers.erase(rit);
        }
      }

      auto key = KeyOf(inst);
      // Only single-def results participate: a multiply-defined result cannot be
      // safely folded away, and a multiply-defined canonical is not a stable value.
      if (key.empty() || !single_def(res)) {
        continue;
      }
      auto it = seen.find(key);
      if (it != seen.end()) {
        replace[res] = it->second;
        continue;
      }
      seen[key] = res;  // canonical (single-def, hence stable)
      for (auto *op : inst->GetOperands()) {
        if (op->IsVirtualReg()) {
          readers[static_cast<TargetVirtualReg *>(op)->GetID()].push_back(key);
        }
      }
    }
  }
  if (replace.empty()) {
    return false;
  }
  // Rewrite uses onto the canonical value; the canonical is never a duplicate, so
  // no chaining is needed. Then drop the now-dead duplicate definitions.
  for (auto *block : func->GetBlocks()) {
    std::vector<TargetInst *> kept;
    kept.reserve(block->GetInsts().size());
    for (auto *inst : block->GetInsts()) {
      if (replace.count(inst->GetResult()) != 0) {
        continue;  // redundant definition
      }
      std::vector<TargetOperand *> ops = inst->GetOperands();
      for (auto *&op : ops) {
        auto it = replace.find(op);
        if (it != replace.end()) {
          op = it->second;
        }
      }
      inst->SetOperands(ops);
      kept.push_back(inst);
    }
    block->SetInsts(kept);
  }
  return true;
}

}  // namespace backend::targetir::aarch64
