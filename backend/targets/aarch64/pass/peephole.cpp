#include "backend/targets/aarch64/pass/peephole.h"
#include <cstdlib>
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

auto AArch64Peephole::BuildAnalysis(TargetFunction *func) -> void {
  def_count_.clear();
  use_count_.clear();
  defining_inst_.clear();
  containing_block_.clear();
  const_value_.clear();
  for (auto *block : func->GetBlocks()) {
    for (auto *inst : block->GetInsts()) {
      containing_block_[inst] = block;
      auto *res = inst->GetResult();
      if (res != nullptr) {
        def_count_[res]++;
        defining_inst_[res] = inst;
      }
      for (auto *operand : inst->GetOperands()) {
        if (operand->IsVirtualReg()) {
          use_count_[operand]++;
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

auto AArch64Peephole::TryFoldMultiplyAccumulate(TargetInst *inst, TargetBlock *block) -> bool {
  if (!enable_tree_combine_ || std::getenv("AARCH64_MADD_COMBINE_OFF") != nullptr) {
    return false;
  }
  auto *ai = static_cast<AArch64Inst *>(inst);
  const auto opcode = ai->GetOpcode();
  const auto &ops = inst->GetOperands();
  if ((opcode != A64Opcode::ADD && opcode != A64Opcode::SUB) || ops.size() != 2 || inst->GetResult() == nullptr ||
      !inst->GetResult()->IsVirtualReg()) {
    return false;
  }

  auto try_product = [&](size_t product_index) -> bool {
    auto *product = ops[product_index];
    if (!product->IsVirtualReg() || use_count_[product] != 1 || def_count_[product] != 1) {
      return false;
    }
    auto def_it = defining_inst_.find(product);
    if (def_it == defining_inst_.end() || containing_block_[def_it->second] != block) {
      return false;
    }
    auto *mul = static_cast<AArch64Inst *>(def_it->second);
    const auto &mul_ops = mul->GetOperands();
    if (mul->GetOpcode() != A64Opcode::MUL || mul_ops.size() != 2) {
      return false;
    }

    auto *other = ops[1 - product_index];
    if (!other->IsVirtualReg() || !mul_ops[0]->IsVirtualReg() || !mul_ops[1]->IsVirtualReg()) {
      return false;
    }
    auto *result_reg = static_cast<TargetVirtualReg *>(inst->GetResult());
    auto same_general_width = [&](TargetOperand *operand) {
      auto *reg = static_cast<TargetVirtualReg *>(operand);
      return reg->GetRegClass() == TargetRegClass::General && reg->GetRegSize() == result_reg->GetRegSize();
    };
    if (result_reg->GetRegClass() != TargetRegClass::General || !same_general_width(other) ||
        !same_general_width(mul_ops[0]) || !same_general_width(mul_ops[1])) {
      return false;
    }

    if (opcode == A64Opcode::ADD) {
      ai->SetOpcode(A64Opcode::MADD);
    } else if (product_index == 1) {
      // AArch64 msub computes ra - rn * rm, matching `other - product`.
      ai->SetOpcode(A64Opcode::MSUB);
    } else {
      return false;  // `product - other` has no direct integer fused form.
    }
    inst->SetOperands({mul_ops[0], mul_ops[1], other});
    return true;
  };

  if (opcode == A64Opcode::SUB) {
    return try_product(1);
  }
  return try_product(0) || try_product(1);
}

auto AArch64Peephole::TryFoldShiftedArithmetic(TargetInst *inst, TargetBlock *block) -> bool {
  if (!enable_tree_combine_ || std::getenv("AARCH64_SHIFTED_ARITH_COMBINE_OFF") != nullptr) {
    return false;
  }
  auto *ai = static_cast<AArch64Inst *>(inst);
  const auto opcode = ai->GetOpcode();
  const auto &ops = inst->GetOperands();
  if ((opcode != A64Opcode::ADD && opcode != A64Opcode::SUB) || ops.size() != 2 || inst->GetResult() == nullptr ||
      !inst->GetResult()->IsVirtualReg()) {
    return false;
  }

  auto try_shift = [&](size_t shifted_index) -> bool {
    auto *shifted = ops[shifted_index];
    if (!shifted->IsVirtualReg() || use_count_[shifted] != 1 || def_count_[shifted] != 1) {
      return false;
    }
    auto def_it = defining_inst_.find(shifted);
    if (def_it == defining_inst_.end() || containing_block_[def_it->second] != block) {
      return false;
    }
    auto *shift = static_cast<AArch64Inst *>(def_it->second);
    const auto shift_opcode = shift->GetOpcode();
    const auto &shift_ops = shift->GetOperands();
    if ((shift_opcode != A64Opcode::LSL_I && shift_opcode != A64Opcode::LSR_I && shift_opcode != A64Opcode::ASR_I) ||
        shift_ops.size() != 2 || shift_ops[1]->GetOpType() != TargetOperandType::Immediate ||
        !shift_ops[0]->IsVirtualReg()) {
      return false;
    }

    auto *result = static_cast<TargetVirtualReg *>(inst->GetResult());
    auto *plain = ops[1 - shifted_index];
    if (!plain->IsVirtualReg()) {
      return false;
    }
    auto same_general_width = [&](TargetOperand *operand) {
      auto *reg = static_cast<TargetVirtualReg *>(operand);
      return reg->GetRegClass() == TargetRegClass::General && reg->GetRegSize() == result->GetRegSize();
    };
    if (result->GetRegClass() != TargetRegClass::General || !same_general_width(shifted) ||
        !same_general_width(plain) || !same_general_width(shift_ops[0])) {
      return false;
    }
    int amount = static_cast<TargetIntLiteral *>(shift_ops[1])->GetValue();
    int max_amount = result->GetRegSize() == TargetRegSize::QWord ? 63 : 31;
    if (amount < 0 || amount > max_amount) {
      return false;
    }

    if (opcode == A64Opcode::SUB && shifted_index == 0) {
      return false;  // `(shifted x) - y` cannot swap its operands.
    }
    if (shift_opcode == A64Opcode::LSL_I) {
      ai->SetOpcode(opcode == A64Opcode::ADD ? A64Opcode::ADD_SHIFT_LSL : A64Opcode::SUB_SHIFT_LSL);
    } else if (shift_opcode == A64Opcode::LSR_I) {
      ai->SetOpcode(opcode == A64Opcode::ADD ? A64Opcode::ADD_SHIFT_LSR : A64Opcode::SUB_SHIFT_LSR);
    } else {
      ai->SetOpcode(opcode == A64Opcode::ADD ? A64Opcode::ADD_SHIFT_ASR : A64Opcode::SUB_SHIFT_ASR);
    }
    inst->SetOperands({plain, shift_ops[0], shift_ops[1]});
    return true;
  };

  if (opcode == A64Opcode::SUB) {
    return try_shift(1);
  }
  return try_shift(0) || try_shift(1);
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

auto AArch64Peephole::TransformInst(TargetInst *inst, TargetBlock *block) -> bool {
  auto *ai = static_cast<AArch64Inst *>(inst);
  auto opcode = ai->GetOpcode();
  const auto &ops = inst->GetOperands();
  if (TryFoldMultiplyAccumulate(inst, block)) {
    return true;
  }
  if (TryFoldShiftedArithmetic(inst, block)) {
    return true;
  }
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
  BuildAnalysis(func);
  bool changed = false;
  for (auto *block : func->GetBlocks()) {
    for (auto *inst : block->GetInsts()) {
      changed |= TransformInst(inst, block);
    }
  }
  return changed;
}

}  // namespace backend::targetir::aarch64
