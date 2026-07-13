#include "backend/targets/aarch64/pass/branchfold.h"
#include <vector>
#include "backend/targets/aarch64/inst/aarch64inst.h"
#include "backend/targets/utils/operand/targetsymbol.h"
#include "backend/targets/utils/struct/targetblock.h"
#include "backend/targets/utils/struct/targetfunction.h"

namespace backend::targetir::aarch64 {

auto AArch64BranchFold::RunOnFunction(TargetFunction *func) -> bool {
  const auto &blocks = func->GetBlocks();
  bool changed = false;
  for (size_t i = 0; i + 1 < blocks.size(); ++i) {
    auto *block = blocks[i];
    const auto &insts = block->GetInsts();
    if (insts.empty()) {
      continue;
    }
    auto *last = static_cast<AArch64Inst *>(insts.back());
    if (last->GetOpcode() != A64Opcode::B) {
      continue;
    }
    const auto &ops = last->GetOperands();
    if (ops.empty() || ops[0]->GetOpType() != TargetOperandType::Label) {
      continue;
    }
    if (static_cast<TargetLabel *>(ops[0])->GetName() == blocks[i + 1]->GetLabel()) {
      std::vector<TargetInst *> kept(insts.begin(), insts.end() - 1);
      block->SetInsts(kept);
      changed = true;
    }
  }
  return changed;
}

}  // namespace backend::targetir::aarch64
