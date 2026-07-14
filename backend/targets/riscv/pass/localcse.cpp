#include "backend/targets/riscv/pass/localcse.h"
#include <string>
#include <unordered_map>
#include <vector>
#include "backend/targets/riscv/inst/riscvinst.h"
#include "backend/targets/utils/operand/targetliteral.h"
#include "backend/targets/utils/operand/targetreg.h"
#include "backend/targets/utils/operand/targetsymbol.h"
#include "backend/targets/utils/struct/targetblock.h"
#include "backend/targets/utils/struct/targetfunction.h"

namespace backend::targetir::riscv {

namespace {
// Pure register-to-register opcodes: a deterministic function of their operands
// with no memory or control-flow effect. Loads/stores, branches, calls, ret and
// the comment pseudo are intentionally absent. `li`/`la` materialize a constant
// or address and are pure functions of their immediate/symbol operand.
auto IsPure(RVOpcode op) -> bool {
  switch (op) {
    case RVOpcode::ADDW:
    case RVOpcode::SUBW:
    case RVOpcode::MULW:
    case RVOpcode::DIVW:
    case RVOpcode::REMW:
    case RVOpcode::ADD:
    case RVOpcode::SUB:
    case RVOpcode::MUL:
    case RVOpcode::AND:
    case RVOpcode::OR:
    case RVOpcode::XOR:
    case RVOpcode::SLLW:
    case RVOpcode::SRLW:
    case RVOpcode::SRAW:
    case RVOpcode::SLT:
    case RVOpcode::SLTU:
    case RVOpcode::ADDIW:
    case RVOpcode::ADDI:
    case RVOpcode::ANDI:
    case RVOpcode::ORI:
    case RVOpcode::XORI:
    case RVOpcode::SLLIW:
    case RVOpcode::SRLIW:
    case RVOpcode::SRAIW:
    case RVOpcode::SLLI:
    case RVOpcode::SLTI:
    case RVOpcode::SLTIU:
    case RVOpcode::MV:
    case RVOpcode::NEGW:
    case RVOpcode::SEQZ:
    case RVOpcode::SNEZ:
    case RVOpcode::SEXT_W:
    case RVOpcode::LI:
    case RVOpcode::LA:
    case RVOpcode::FADD_S:
    case RVOpcode::FSUB_S:
    case RVOpcode::FMUL_S:
    case RVOpcode::FDIV_S:
    case RVOpcode::FEQ_S:
    case RVOpcode::FLT_S:
    case RVOpcode::FLE_S:
    case RVOpcode::FMV_S:
    case RVOpcode::FCVT_S_W:
    case RVOpcode::FCVT_W_S:
    case RVOpcode::FMV_W_X:
    case RVOpcode::FMV_X_W:
    case RVOpcode::SRAI:
      return true;
    default:
      return false;
  }
}
}  // namespace

auto RISCVLocalCSE::KeyOf(TargetInst *inst) -> std::string {
  auto *ri = static_cast<RISCVInst *>(inst);
  if (!IsPure(ri->GetOpcode())) {
    return "";
  }
  auto *res = inst->GetResult();
  if (res == nullptr || !res->IsVirtualReg()) {
    return "";
  }
  // The result's width/class is not derivable from the operands (a move/convert
  // can target a differently-sized or -classed view of the same source), so it
  // must be part of the key -- otherwise two same-operand ops with differing
  // results merge wrongly.
  auto *rv = static_cast<TargetVirtualReg *>(res);
  std::string key = std::to_string(static_cast<int>(ri->GetOpcode()));
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

auto RISCVLocalCSE::RunOnFunction(TargetFunction *func) -> bool {
  // Virtual registers here are NOT single-assignment: loop-carried values (phis)
  // and accumulators are rewritten, so the same vreg can be defined many times.
  // Count definitions so we only ever *replace* a single-def result (safe to RAUW
  // and delete) and only trust a single-def canonical (its value never changes).
  // The address-arithmetic temporaries (sext.w/slli/add results) are single-def.
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

}  // namespace backend::targetir::riscv
