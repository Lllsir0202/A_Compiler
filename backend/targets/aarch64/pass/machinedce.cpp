#include "backend/targets/aarch64/pass/machinedce.h"
#include <unordered_map>
#include <vector>
#include "backend/targets/aarch64/inst/aarch64inst.h"
#include "backend/targets/utils/operand/targetreg.h"
#include "backend/targets/utils/struct/targetblock.h"
#include "backend/targets/utils/struct/targetfunction.h"

namespace backend::targetir::aarch64 {

auto AArch64MachineDCE::RunOnFunction(TargetFunction *func) -> bool {
  bool changed_overall = false;
  bool changed = true;
  while (changed) {
    changed = false;
    std::unordered_map<TargetOperand *, int> use_count;
    for (auto *block : func->GetBlocks()) {
      for (auto *inst : block->GetInsts()) {
        for (auto *op : inst->GetOperands()) {
          if (op->IsVirtualReg() || op->IsPhysicalReg()) {
            use_count[op]++;
          }
        }
      }
    }
    for (auto *block : func->GetBlocks()) {
      std::vector<TargetInst *> kept;
      kept.reserve(block->GetInsts().size());
      for (auto *inst : block->GetInsts()) {
        auto *res = inst->GetResult();
        auto opcode = static_cast<AArch64Inst *>(inst)->GetOpcode();
        bool dead =
            res != nullptr && res->IsVirtualReg() && opcode != A64Opcode::BL && use_count.find(res) == use_count.end();
        if (dead) {
          changed = true;
          changed_overall = true;
          continue;
        }
        kept.push_back(inst);
      }
      block->SetInsts(kept);
    }
  }
  return changed_overall;
}

}  // namespace backend::targetir::aarch64
