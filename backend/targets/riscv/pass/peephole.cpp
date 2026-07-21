#include "backend/targets/riscv/pass/peephole.h"
#include <cstdint>
#include <limits>
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

auto RISCVPeephole::BuildFacts(TargetFunction *func) -> void {
  def_count_.clear();
  use_count_.clear();
  li_value_.clear();
  defining_inst_.clear();
  for (auto *block : func->GetBlocks()) {
    for (auto *inst : block->GetInsts()) {
      auto *res = inst->GetResult();
      if (res != nullptr) {
        def_count_[res]++;
        defining_inst_[res] = inst;
      }
      for (auto *operand : inst->GetOperands()) {
        if (operand != nullptr && operand->IsVirtualReg()) {
          use_count_[operand]++;
        }
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

auto RISCVPeephole::HasSignExtendedWordResult(TargetOperand *op) const -> bool {
  if (op == nullptr || !op->IsVirtualReg()) {
    return false;
  }
  auto count = def_count_.find(op);
  if (count == def_count_.end() || count->second != 1) {
    return false;
  }
  auto def = defining_inst_.find(op);
  if (def == defining_inst_.end()) {
    return false;
  }

  // Every W-form integer result, LW, and FCVT.W.S is sign-extended to XLEN by
  // the RV64 ISA. Comparisons and boolean pseudos produce only zero or one.
  // LI is also safe here: TargetIntLiteral is an int, so its value is already
  // exactly the sign extension of an i32 value.
  switch (static_cast<RISCVInst *>(def->second)->GetOpcode()) {
    case RVOpcode::ADDW:
    case RVOpcode::SUBW:
    case RVOpcode::MULW:
    case RVOpcode::DIVW:
    case RVOpcode::REMW:
    case RVOpcode::SLLW:
    case RVOpcode::SRLW:
    case RVOpcode::SRAW:
    case RVOpcode::ADDIW:
    case RVOpcode::SLLIW:
    case RVOpcode::SRLIW:
    case RVOpcode::SRAIW:
    case RVOpcode::NEGW:
    case RVOpcode::SEXT_W:
    case RVOpcode::LW:
    case RVOpcode::FCVT_W_S:
    case RVOpcode::SLT:
    case RVOpcode::SLTU:
    case RVOpcode::SLTI:
    case RVOpcode::SLTIU:
    case RVOpcode::SEQZ:
    case RVOpcode::SNEZ:
    case RVOpcode::FEQ_S:
    case RVOpcode::FLT_S:
    case RVOpcode::FLE_S:
    case RVOpcode::LI:
      return true;
    default:
      return false;
  }
}

auto RISCVPeephole::FoldBranchZero(TargetInst *inst) -> bool {
  auto opcode = static_cast<RISCVInst *>(inst)->GetOpcode();
  bool is_equality_branch = opcode == RVOpcode::BEQ || opcode == RVOpcode::BNE;
  const auto &ops = inst->GetOperands();
  if (!is_equality_branch || ops.size() != 3) {
    return false;
  }

  auto *lhs = ops[0];
  auto *rhs = ops[1];
  int value = 0;
  bool lhs_is_zero = IsConst(lhs, &value) && value == 0;
  bool rhs_is_zero = IsConst(rhs, &value) && value == 0;
  if (!lhs_is_zero && !rhs_is_zero) {
    return false;
  }

  auto *zero = ctx_->PhysReg(TargetRegClass::General, TargetRegSize::QWord, 0);
  inst->SetOperands({lhs_is_zero ? zero : lhs, rhs_is_zero ? zero : rhs, ops[2]});
  return true;
}

auto RISCVPeephole::TransformInst(TargetInst *inst) -> bool {
  auto *ri = static_cast<RISCVInst *>(inst);
  auto opcode = ri->GetOpcode();
  const auto &ops = inst->GetOperands();
  if (opcode == RVOpcode::SEXT_W && ops.size() == 1 && HasSignExtendedWordResult(ops[0])) {
    // Keep the destination SSA value but expose a coalescible copy. The graph
    // allocator removes the move when source and destination receive one color.
    ri->SetOpcode(RVOpcode::MV);
    return true;
  }
  if (FoldBranchZero(inst)) {
    return true;
  }
  if (ops.size() != 2) {
    return false;  // only the two-register R-type forms are handled here
  }
  auto *lhs = ops[0];
  auto *rhs = ops[1];
  int c = 0;

  // Signed-magic division first extracts the high 32 product bits, then
  // applies a word shift:
  //
  //   hi = srai product, 32
  //   q  = sraiw hi, k
  //
  // With a single use of `hi`, this is exactly `product >> (32 + k)`.
  // MachineDCE removes the first shift after this rewrite.
  if (opcode == RVOpcode::SRAIW && rhs->GetOpType() == TargetOperandType::Immediate && lhs->IsVirtualReg() &&
      use_count_[lhs] == 1) {
    auto def_iter = defining_inst_.find(lhs);
    if (def_iter != defining_inst_.end()) {
      auto *def = static_cast<RISCVInst *>(def_iter->second);
      const auto &def_ops = def->GetOperands();
      int shift = static_cast<TargetIntLiteral *>(rhs)->GetValue();
      if (def->GetOpcode() == RVOpcode::SRAI && def_ops.size() == 2 &&
          def_ops[1]->GetOpType() == TargetOperandType::Immediate &&
          static_cast<TargetIntLiteral *>(def_ops[1])->GetValue() == 32 && shift >= 0 && shift <= 31) {
        ri->SetOpcode(RVOpcode::SRAI);
        inst->SetOperands({def_ops[0], ctx_->Imm(32 + shift)});
        return true;
      }
    }
  }

  auto constant_value = [&](TargetOperand *operand, int *value) -> bool {
    if (operand->GetOpType() == TargetOperandType::Immediate) {
      *value = static_cast<TargetIntLiteral *>(operand)->GetValue();
      return true;
    }
    return IsConst(operand, value);
  };
  auto to_constant = [&](int value) {
    ri->SetOpcode(RVOpcode::LI);
    inst->SetOperands({ctx_->Imm(value)});
  };

  // Fold a fully constant machine expression. The middle-end unroller creates
  // constant GEP indices as SSA values, so instruction selection otherwise
  // emits `li index; li stride; mul/slli; add` even though the byte offset is a
  // compile-time integer. Collapsing the chain here exposes ADDI-based address
  // expressions to RISCVAddrFold in the next pipeline iteration.
  int lhs_value = 0;
  int rhs_value = 0;
  if (constant_value(lhs, &lhs_value) && constant_value(rhs, &rhs_value)) {
    auto as_i32 = [](uint32_t value) { return static_cast<int32_t>(value); };
    switch (opcode) {
      case RVOpcode::ADDW:
        to_constant(as_i32(static_cast<uint32_t>(lhs_value) + static_cast<uint32_t>(rhs_value)));
        return true;
      case RVOpcode::SUBW:
        to_constant(as_i32(static_cast<uint32_t>(lhs_value) - static_cast<uint32_t>(rhs_value)));
        return true;
      case RVOpcode::MULW:
        to_constant(as_i32(static_cast<uint32_t>(lhs_value) * static_cast<uint32_t>(rhs_value)));
        return true;
      case RVOpcode::ADD:
      case RVOpcode::ADDI: {
        int64_t value = static_cast<int64_t>(lhs_value) + rhs_value;
        if (value >= std::numeric_limits<int>::min() && value <= std::numeric_limits<int>::max()) {
          to_constant(static_cast<int>(value));
          return true;
        }
        break;
      }
      case RVOpcode::SUB: {
        int64_t value = static_cast<int64_t>(lhs_value) - rhs_value;
        if (value >= std::numeric_limits<int>::min() && value <= std::numeric_limits<int>::max()) {
          to_constant(static_cast<int>(value));
          return true;
        }
        break;
      }
      case RVOpcode::MUL: {
        int64_t value = static_cast<int64_t>(lhs_value) * rhs_value;
        if (value >= std::numeric_limits<int>::min() && value <= std::numeric_limits<int>::max()) {
          to_constant(static_cast<int>(value));
          return true;
        }
        break;
      }
      case RVOpcode::AND: to_constant(lhs_value & rhs_value); return true;
      case RVOpcode::OR: to_constant(lhs_value | rhs_value); return true;
      case RVOpcode::XOR: to_constant(lhs_value ^ rhs_value); return true;
      case RVOpcode::SLLW:
        to_constant(as_i32(static_cast<uint32_t>(lhs_value) << (static_cast<unsigned>(rhs_value) & 31U)));
        return true;
      case RVOpcode::SRLW:
        to_constant(as_i32(static_cast<uint32_t>(lhs_value) >> (static_cast<unsigned>(rhs_value) & 31U)));
        return true;
      case RVOpcode::SRAW:
        to_constant(static_cast<int32_t>(lhs_value) >> (static_cast<unsigned>(rhs_value) & 31U));
        return true;
      case RVOpcode::SLLI: {
        uint64_t value = static_cast<uint64_t>(static_cast<int64_t>(lhs_value))
                         << (static_cast<unsigned>(rhs_value) & 63U);
        if (value <= static_cast<uint64_t>(std::numeric_limits<int>::max())) {
          to_constant(static_cast<int>(value));
          return true;
        }
        break;
      }
      case RVOpcode::SLLIW:
        to_constant(as_i32(static_cast<uint32_t>(lhs_value) << (static_cast<unsigned>(rhs_value) & 31U)));
        return true;
      case RVOpcode::SRLIW:
        to_constant(as_i32(static_cast<uint32_t>(lhs_value) >> (static_cast<unsigned>(rhs_value) & 31U)));
        return true;
      case RVOpcode::SRAIW:
        to_constant(static_cast<int32_t>(lhs_value) >> (static_cast<unsigned>(rhs_value) & 31U));
        return true;
      case RVOpcode::SLT: to_constant(lhs_value < rhs_value ? 1 : 0); return true;
      case RVOpcode::SLTU:
        to_constant(static_cast<uint32_t>(lhs_value) < static_cast<uint32_t>(rhs_value) ? 1 : 0);
        return true;
      default: break;
    }
  }

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
  BuildFacts(func);
  bool changed = false;
  for (auto *block : func->GetBlocks()) {
    for (auto *inst : block->GetInsts()) {
      changed |= TransformInst(inst);
    }
  }
  return changed;
}

}  // namespace backend::targetir::riscv
