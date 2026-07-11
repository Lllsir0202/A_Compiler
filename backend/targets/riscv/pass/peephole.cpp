#include "backend/targets/riscv/pass/peephole.h"
#include <vector>
#include "backend/targets/utils/context/targetcontext.h"
#include "backend/targets/utils/operand/targetliteral.h"
#include "backend/targets/utils/operand/targetreg.h"
#include "backend/targets/utils/struct/targetblock.h"
#include "backend/targets/utils/struct/targetfunction.h"

namespace backend::targetir::riscv {

namespace {
constexpr int IMM_MIN = -2048;
constexpr int IMM_MAX = 2047;

auto InImmRange(int value) -> bool { return value >= IMM_MIN && value <= IMM_MAX; }

// True for C > 0 that is an exact power of two; *shift receives log2(C).
auto IsPowerOfTwo(int value, int *shift) -> bool {
  if (value <= 0 || (value & (value - 1)) != 0) {
    return false;
  }
  *shift = __builtin_ctz(static_cast<unsigned>(value));
  return true;
}
}  // namespace

auto RISCVPeephole::BuildConstants(TargetFunction *func) -> void {
  def_count_.clear();
  li_value_.clear();
  for (auto *block : func->GetBlocks()) {
    for (auto *inst : block->GetInsts()) {
      auto *res = inst->GetResult();
      if (res != nullptr) {
        def_count_[res]++;
      }
      auto *ri = static_cast<RISCVInst *>(inst);
      if (ri->GetOpcode() == RVOpcode::LI && res != nullptr) {
        const auto &ops = inst->GetOperands();
        if (!ops.empty() && ops[0]->GetOpType() == TargetOperandType::Immediate) {
          li_value_[res] = static_cast<TargetIntLiteral *>(ops[0])->GetValue();
        }
      }
    }
  }
}

auto RISCVPeephole::IsConst(TargetOperand *op, int *value) const -> bool {
  if (op == nullptr || !op->IsVirtualReg()) {
    return false;
  }
  auto def_it = def_count_.find(op);
  if (def_it == def_count_.end() || def_it->second != 1) {
    return false;  // multiply-defined: not a stable constant
  }
  auto it = li_value_.find(op);
  if (it == li_value_.end()) {
    return false;
  }
  *value = it->second;
  return true;
}

auto RISCVPeephole::TransformInst(TargetInst *inst) -> bool {
  auto *ri = static_cast<RISCVInst *>(inst);
  auto opcode = ri->GetOpcode();
  const auto &ops = inst->GetOperands();
  if (ops.size() != 2) {
    return false;  // only the two-register R-type forms are handled here
  }
  auto *lhs = ops[0];
  auto *rhs = ops[1];
  int c = 0;

  // Rewrite `inst` in place to a unary op (mv / negw / li) on `reg`.
  auto to_move = [&](TargetOperand *reg) {
    ri->SetOpcode(RVOpcode::MV);
    inst->SetOperands({reg});
  };
  // Rewrite to an I-type: `mnem rd, reg, imm`.
  auto to_immediate = [&](RVOpcode op, TargetOperand *reg, int imm) {
    ri->SetOpcode(op);
    inst->SetOperands({reg, ctx_->Imm(imm)});
  };

  switch (opcode) {
    // --- multiply: strength reduction (no register*immediate on RISC-V) ---
    case RVOpcode::MULW:
    case RVOpcode::MUL: {
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
        ri->SetOpcode(RVOpcode::LI);
        inst->SetOperands({ctx_->Imm(0)});
        return true;
      }
      if (c == 1) {
        to_move(reg);
        return true;
      }
      if (IsPowerOfTwo(c, &shift)) {
        to_immediate(opcode == RVOpcode::MULW ? RVOpcode::SLLIW : RVOpcode::SLLI, reg, shift);
        return true;
      }
      return false;
    }
    // --- commutative add: fold either constant operand ---
    case RVOpcode::ADDW:
    case RVOpcode::ADD: {
      auto imm_op = opcode == RVOpcode::ADDW ? RVOpcode::ADDIW : RVOpcode::ADDI;
      if (IsConst(rhs, &c) && InImmRange(c)) {
        to_immediate(imm_op, lhs, c);
        return true;
      }
      if (IsConst(lhs, &c) && InImmRange(c)) {
        to_immediate(imm_op, rhs, c);
        return true;
      }
      return false;
    }
    // --- subtract: only the subtrahend can fold (a - C == a + (-C)) ---
    case RVOpcode::SUBW:
    case RVOpcode::SUB: {
      if (IsConst(rhs, &c) && InImmRange(-c)) {
        to_immediate(opcode == RVOpcode::SUBW ? RVOpcode::ADDIW : RVOpcode::ADDI, lhs, -c);
        return true;
      }
      return false;
    }
    case RVOpcode::AND: {
      if (IsConst(rhs, &c) && InImmRange(c)) {
        to_immediate(RVOpcode::ANDI, lhs, c);
        return true;
      }
      if (IsConst(lhs, &c) && InImmRange(c)) {
        to_immediate(RVOpcode::ANDI, rhs, c);
        return true;
      }
      return false;
    }
    case RVOpcode::OR: {
      if (IsConst(rhs, &c) && InImmRange(c)) {
        to_immediate(RVOpcode::ORI, lhs, c);
        return true;
      }
      if (IsConst(lhs, &c) && InImmRange(c)) {
        to_immediate(RVOpcode::ORI, rhs, c);
        return true;
      }
      return false;
    }
    case RVOpcode::XOR: {
      if (IsConst(rhs, &c) && InImmRange(c)) {
        to_immediate(RVOpcode::XORI, lhs, c);
        return true;
      }
      if (IsConst(lhs, &c) && InImmRange(c)) {
        to_immediate(RVOpcode::XORI, rhs, c);
        return true;
      }
      return false;
    }
    // --- shifts: only the (small) shift amount can be an immediate ---
    case RVOpcode::SLLW:
    case RVOpcode::SRLW:
    case RVOpcode::SRAW: {
      if (IsConst(rhs, &c) && c >= 0 && c <= 31) {
        RVOpcode imm_op = opcode == RVOpcode::SLLW   ? RVOpcode::SLLIW
                          : opcode == RVOpcode::SRLW ? RVOpcode::SRLIW
                                                     : RVOpcode::SRAIW;
        to_immediate(imm_op, lhs, c);
        return true;
      }
      return false;
    }
    case RVOpcode::SLT: {
      if (IsConst(rhs, &c) && InImmRange(c)) {
        to_immediate(RVOpcode::SLTI, lhs, c);
        return true;
      }
      return false;
    }
    case RVOpcode::SLTU: {
      if (IsConst(rhs, &c) && InImmRange(c)) {
        to_immediate(RVOpcode::SLTIU, lhs, c);
        return true;
      }
      return false;
    }
    // --- algebraic identities left behind by folding ---
    case RVOpcode::ADDIW:
    case RVOpcode::ADDI:
    case RVOpcode::ORI:
    case RVOpcode::XORI:
    case RVOpcode::SLLIW:
    case RVOpcode::SRLIW:
    case RVOpcode::SRAIW: {
      if (rhs->GetOpType() == TargetOperandType::Immediate &&
          static_cast<TargetIntLiteral *>(rhs)->GetValue() == 0) {
        to_move(lhs);
        return true;
      }
      return false;
    }
    case RVOpcode::ANDI: {
      if (rhs->GetOpType() == TargetOperandType::Immediate &&
          static_cast<TargetIntLiteral *>(rhs)->GetValue() == -1) {
        to_move(lhs);  // AND with all-ones is the identity
        return true;
      }
      return false;
    }
    default:
      return false;
  }
}

auto RISCVPeephole::RunOnFunction(TargetFunction *func) -> bool {
  BuildConstants(func);
  bool changed = false;
  for (auto *block : func->GetBlocks()) {
    for (auto *inst : block->GetInsts()) {
      changed |= TransformInst(inst);
    }
  }
  return changed;
}

}  // namespace backend::targetir::riscv
