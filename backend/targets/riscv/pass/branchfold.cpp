#include "backend/targets/riscv/pass/branchfold.h"
#include <vector>
#include "backend/targets/riscv/inst/riscvinst.h"
#include "backend/targets/utils/operand/targetsymbol.h"
#include "backend/targets/utils/struct/targetblock.h"
#include "backend/targets/utils/struct/targetfunction.h"

namespace backend::targetir::riscv {

auto BranchFold::RunOnFunction(TargetFunction *func) -> bool {
  const auto &blocks = func->GetBlocks();
  bool changed = false;
  for (size_t i = 0; i + 1 < blocks.size(); ++i) {
    auto *block = blocks[i];
    const auto &insts = block->GetInsts();
    if (insts.empty()) {
      continue;
    }
    auto *last = static_cast<RISCVInst *>(insts.back());
    if (last->GetOpcode() != RVOpcode::J) {
      continue;
    }
    const auto &ops = last->GetOperands();
    if (ops.empty() || ops[0]->GetOpType() != TargetOperandType::Label) {
      continue;
    }
    // The jump targets the very next block in layout order: it is redundant.
    if (static_cast<TargetLabel *>(ops[0])->GetName() == blocks[i + 1]->GetLabel()) {
      std::vector<TargetInst *> kept(insts.begin(), insts.end() - 1);
      block->SetInsts(kept);
      changed = true;
    }
  }
  return changed;
}

}  // namespace backend::targetir::riscv
