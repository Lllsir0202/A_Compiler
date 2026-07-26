#include "backend/targets/riscv/pass/machinedce.h"
#include <unordered_map>
#include <vector>
#include "backend/targets/riscv/inst/riscvinst.h"
#include "backend/targets/utils/operand/targetreg.h"
#include "backend/targets/utils/struct/targetblock.h"
#include "backend/targets/utils/struct/targetfunction.h"

namespace backend::targetir::riscv {

auto MachineDCE::RunOnFunction(TargetFunction *func) -> bool {
  bool changed_overall = false;
  bool changed = true;
  while (changed) {
    changed = false;
    // Count how many times each register operand is used.
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
    // Phi incoming materializations live on CFG edges until out-of-SSA
    // translation. Their operands are real uses of block-defined values and
    // must keep those definitions alive.
    for (const auto &[edge, instructions] : func->GetAllEdgeCode()) {
      (void)edge;
      for (auto *inst : instructions) {
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
        auto opcode = static_cast<RISCVInst *>(inst)->GetOpcode();
        // Removable iff it defines an otherwise-unused virtual register and has no
        // side effect. Opcode semantics come from the shared descriptor table,
        // not a pass-local list that can silently go stale when instructions are
        // added.
        bool dead = res != nullptr && res->IsVirtualReg() && !GetRVOpcodeInfo(opcode).HasFlag(RVInstFlag::SideEffect) &&
                    use_count.find(res) == use_count.end();
        if (dead) {
          changed = true;
          changed_overall = true;
          continue;  // drop it
        }
        kept.push_back(inst);
      }
      block->SetInsts(kept);
    }
  }
  return changed_overall;
}

}  // namespace backend::targetir::riscv
