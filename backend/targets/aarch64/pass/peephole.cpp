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

auto AArch64Peephole::BuildFacts(TargetFunction *func) -> void {
  def_count_.clear();
  use_count_.clear();
  defining_inst_.clear();
  previous_inst_.clear();
  const_value_.clear();
  for (auto *block : func->GetBlocks()) {
    TargetInst *previous = nullptr;
    for (auto *inst : block->GetInsts()) {
      previous_inst_[inst] = previous;
      previous = inst;
      auto *res = inst->GetResult();
      if (res != nullptr) {
        def_count_[res]++;
        defining_inst_[res] = inst;
      }
      for (auto *op : inst->GetOperands()) {
        if (op->IsVirtualReg()) {
          use_count_[op]++;
        }
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

auto AArch64Peephole::SingleUseMultiply(TargetOperand *op) const -> TargetInst * {
  if (op == nullptr || !op->IsVirtualReg()) {
    return nullptr;
  }
  auto def_count = def_count_.find(op);
  auto use_count = use_count_.find(op);
  auto defining_inst = defining_inst_.find(op);
  if (def_count == def_count_.end() || def_count->second != 1 || use_count == use_count_.end() ||
      use_count->second != 1 || defining_inst == defining_inst_.end()) {
    return nullptr;
  }
  auto *mul = static_cast<AArch64Inst *>(defining_inst->second);
  return mul->GetOpcode() == A64Opcode::MUL && mul->GetOperands().size() == 2 ? mul : nullptr;
}

auto AArch64Peephole::FoldMultiplyAccumulate(TargetInst *inst) -> bool {
  auto *ai = static_cast<AArch64Inst *>(inst);
  auto opcode = ai->GetOpcode();
  const auto &ops = inst->GetOperands();
  if (ops.size() != 2 || (opcode != A64Opcode::ADD && opcode != A64Opcode::SUB)) {
    return false;
  }

  TargetInst *mul = nullptr;
  TargetOperand *addend = nullptr;
  if (opcode == A64Opcode::ADD) {
    mul = SingleUseMultiply(ops[0]);
    addend = ops[1];
    if (mul == nullptr) {
      mul = SingleUseMultiply(ops[1]);
      addend = ops[0];
    }
  } else {
    // AArch64 msub computes Ra - Rn*Rm, so only a multiply in the subtrahend folds.
    mul = SingleUseMultiply(ops[1]);
    addend = ops[0];
  }
  if (mul == nullptr) {
    return false;
  }
  auto previous = previous_inst_.find(inst);
  if (previous == previous_inst_.end() || previous->second != mul) {
    return false;
  }

  const auto &mul_ops = mul->GetOperands();
  ai->SetOpcode(opcode == A64Opcode::ADD ? A64Opcode::MADD : A64Opcode::MSUB);
  inst->SetOperands({mul_ops[0], mul_ops[1], addend});
  return true;
}

auto AArch64Peephole::TransformInst(TargetInst *inst) -> bool {
  auto *ai = static_cast<AArch64Inst *>(inst);
  auto opcode = ai->GetOpcode();
  const auto &ops = inst->GetOperands();
  if (ops.size() != 2) {
    return false;
  }
  if (FoldMultiplyAccumulate(inst)) {
    return true;
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
      if (rhs->GetOpType() == TargetOperandType::Immediate && static_cast<TargetIntLiteral *>(rhs)->GetValue() == 0) {
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
  BuildFacts(func);
  bool changed = false;
  for (auto *block : func->GetBlocks()) {
    for (auto *inst : block->GetInsts()) {
      changed |= TransformInst(inst);
    }
  }
  return changed;
}

}  // namespace backend::targetir::aarch64
