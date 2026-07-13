#include "backend/targets/aarch64/pass/peephole.h"
#include <vector>
#include "backend/targets/utils/context/targetcontext.h"
#include "backend/targets/utils/operand/targetliteral.h"
#include "backend/targets/utils/operand/targetreg.h"
#include "backend/targets/utils/struct/targetblock.h"
#include "backend/targets/utils/struct/targetfunction.h"

namespace backend::targetir::aarch64 {

namespace {
constexpr int IMM12_MAX = 4095;  // arithmetic add/sub/cmp immediate (unshifted)

auto InImm12(int value) -> bool { return value >= 0 && value <= IMM12_MAX; }

auto IsPowerOfTwo(int value, int *shift) -> bool {
  if (value <= 0 || (value & (value - 1)) != 0) {
    return false;
  }
  *shift = __builtin_ctz(static_cast<unsigned>(value));
  return true;
}
}  // namespace

auto AArch64Peephole::BuildConstants(TargetFunction *func) -> void {
  def_count_.clear();
  const_value_.clear();
  for (auto *block : func->GetBlocks()) {
    for (auto *inst : block->GetInsts()) {
      auto *res = inst->GetResult();
      if (res != nullptr) {
        def_count_[res]++;
      }
      auto *ai = static_cast<AArch64Inst *>(inst);
      auto opc = ai->GetOpcode();
      if ((opc == A64Opcode::MOVZ || opc == A64Opcode::MOVN) && res != nullptr) {
        const auto &ops = inst->GetOperands();
        if (!ops.empty() && ops[0]->GetOpType() == TargetOperandType::Immediate) {
          int imm = static_cast<TargetIntLiteral *>(ops[0])->GetValue();
          const_value_[res] = opc == A64Opcode::MOVZ ? imm : ~imm;
        }
      }
    }
  }
}

auto AArch64Peephole::IsConst(TargetOperand *op, int *value) const -> bool {
  if (op == nullptr || !op->IsVirtualReg()) {
    return false;
  }
  auto def_it = def_count_.find(op);
  if (def_it == def_count_.end() || def_it->second != 1) {
    return false;  // multiply-defined (e.g. movz+movk) is not a stable single constant
  }
  auto it = const_value_.find(op);
  if (it == const_value_.end()) {
    return false;
  }
  *value = it->second;
  return true;
}

auto AArch64Peephole::TransformInst(TargetInst *inst) -> bool {
  auto *ai = static_cast<AArch64Inst *>(inst);
  auto opcode = ai->GetOpcode();
  const auto &ops = inst->GetOperands();
  if (ops.size() != 2) {
    return false;
  }
  auto *lhs = ops[0];
  auto *rhs = ops[1];
  int c = 0;

  auto to_move = [&](TargetOperand *reg) {
    ai->SetOpcode(A64Opcode::MOV);
    inst->SetOperands({reg});
  };
  auto to_immediate = [&](A64Opcode op, TargetOperand *reg, int imm) {
    ai->SetOpcode(op);
    inst->SetOperands({reg, ctx_->Imm(imm)});
  };

  switch (opcode) {
    case A64Opcode::MUL: {
      TargetOperand *reg = nullptr;
      if (IsConst(rhs, &c)) {
        reg = lhs;
      } else if (IsConst(lhs, &c)) {
        reg = rhs;
      } else {
        return false;
      }
      int shift = 0;
      if (c == 0) {
        ai->SetOpcode(A64Opcode::MOVZ);
        inst->SetOperands({ctx_->Imm(0)});
        return true;
      }
      if (c == 1) {
        to_move(reg);
        return true;
      }
      if (IsPowerOfTwo(c, &shift)) {
        to_immediate(A64Opcode::LSL_I, reg, shift);
        return true;
      }
      return false;
    }
    case A64Opcode::ADD: {  // commutative
      if (IsConst(rhs, &c) && InImm12(c)) {
        to_immediate(A64Opcode::ADD_I, lhs, c);
        return true;
      }
      if (IsConst(lhs, &c) && InImm12(c)) {
        to_immediate(A64Opcode::ADD_I, rhs, c);
        return true;
      }
      return false;
    }
    case A64Opcode::SUB: {  // only the subtrahend folds: a - C
      if (IsConst(rhs, &c) && InImm12(c)) {
        to_immediate(A64Opcode::SUB_I, lhs, c);
        return true;
      }
      return false;
    }
    case A64Opcode::CMP: {  // cmp rn, rm  ->  cmp rn, #C
      if (IsConst(rhs, &c) && InImm12(c)) {
        to_immediate(A64Opcode::CMP_I, lhs, c);
        return true;
      }
      return false;
    }
    // Algebraic identities left behind by folding.
    case A64Opcode::ADD_I:
    case A64Opcode::SUB_I:
    case A64Opcode::LSL_I:
    case A64Opcode::LSR_I:
    case A64Opcode::ASR_I: {
      if (rhs->GetOpType() == TargetOperandType::Immediate &&
          static_cast<TargetIntLiteral *>(rhs)->GetValue() == 0) {
        to_move(lhs);
        return true;
      }
      return false;
    }
    default:
      return false;
  }
}

auto AArch64Peephole::RunOnFunction(TargetFunction *func) -> bool {
  BuildConstants(func);
  bool changed = false;
  for (auto *block : func->GetBlocks()) {
    for (auto *inst : block->GetInsts()) {
      changed |= TransformInst(inst);
    }
  }
  return changed;
}

}  // namespace backend::targetir::aarch64
