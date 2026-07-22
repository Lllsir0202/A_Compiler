#include "backend/targets/riscv/pass/branchfold.h"
#include <vector>
#include "backend/targets/riscv/inst/riscvinst.h"
#include "backend/targets/utils/operand/targetsymbol.h"
#include "backend/targets/utils/struct/targetblock.h"
#include "backend/targets/utils/struct/targetfunction.h"

namespace backend::targetir::riscv {
namespace {
auto InverseBranch(RVOpcode opcode, RVOpcode *inverse) -> bool {
  switch (opcode) {
    case RVOpcode::BEQ:
      *inverse = RVOpcode::BNE;
      return true;
    case RVOpcode::BNE:
      *inverse = RVOpcode::BEQ;
      return true;
    case RVOpcode::BLT:
      *inverse = RVOpcode::BGE;
      return true;
    case RVOpcode::BGE:
      *inverse = RVOpcode::BLT;
      return true;
    case RVOpcode::BLTU:
      *inverse = RVOpcode::BGEU;
      return true;
    case RVOpcode::BGEU:
      *inverse = RVOpcode::BLTU;
      return true;
    default:
      return false;
  }
}
}  // namespace

auto BranchFold::RunOnFunction(TargetFunction *func) -> bool {
  const auto &blocks = func->GetBlocks();
  bool changed = false;
  for (size_t i = 0; i + 1 < blocks.size(); ++i) {
    auto *block = blocks[i];
    const auto &insts = block->GetInsts();
    if (insts.empty()) {
      continue;
    }

    // Instruction selection emits a conditional branch to the true block and
    // an unconditional jump to the false block. When the true block is already
    // next in layout order, invert the condition and branch directly to false;
    // true then falls through and the unconditional jump disappears.
    if (insts.size() >= 2) {
      auto *branch = static_cast<RISCVInst *>(insts[insts.size() - 2]);
      auto *jump = static_cast<RISCVInst *>(insts.back());
      RVOpcode inverse = RVOpcode::BEQ;
      const auto &branch_ops = branch->GetOperands();
      const auto &jump_ops = jump->GetOperands();
      if (jump->GetOpcode() == RVOpcode::J && InverseBranch(branch->GetOpcode(), &inverse) && branch_ops.size() == 3 &&
          jump_ops.size() == 1 && branch_ops[2]->GetOpType() == TargetOperandType::Label &&
          jump_ops[0]->GetOpType() == TargetOperandType::Label &&
          static_cast<TargetLabel *>(branch_ops[2])->GetName() == blocks[i + 1]->GetLabel()) {
        branch->SetOpcode(inverse);
        branch->SetOperands({branch_ops[0], branch_ops[1], jump_ops[0]});
        std::vector<TargetInst *> kept(insts.begin(), insts.end() - 1);
        block->SetInsts(std::move(kept));
        changed = true;
        continue;
      }
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
