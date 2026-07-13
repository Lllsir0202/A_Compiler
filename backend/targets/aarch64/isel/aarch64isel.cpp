#include "backend/targets/aarch64/isel/aarch64isel.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include "backend/mir/inst/arithm/arithm.h"
#include "backend/mir/inst/branch/branch.h"
#include "backend/mir/inst/branch/ret.h"
#include "backend/mir/inst/cmp/cmp.h"
#include "backend/mir/inst/func/call.h"
#include "backend/mir/inst/func/funcdef.h"
#include "backend/mir/inst/inst.h"
#include "backend/mir/inst/pseudo/copy.h"
#include "backend/mir/utils/memory/frameinfo.h"
#include "backend/mir/utils/operand/frameoperand.h"
#include "backend/mir/utils/operand/globaloperand.h"
#include "backend/mir/utils/operand/labeloperand.h"
#include "backend/mir/utils/operand/literaloperand.h"
#include "backend/mir/utils/operand/memoryoperand.h"
#include "backend/mir/utils/operand/operand.h"
#include "backend/mir/utils/struct/block.h"
#include "backend/mir/utils/struct/function.h"
#include "backend/targets/aarch64/regs/aarch64reg.h"
#include "backend/targets/utils/divmagic.h"
#include "backend/targets/utils/operand/targetreg.h"
#include "context/compiler_context.h"

namespace backend::targetir::aarch64 {

using backend::inst::OpKind;

namespace {
constexpr auto X0 = static_cast<uint8_t>(GPR::X0);
constexpr auto SP = static_cast<uint8_t>(GPR::Sp);
constexpr auto FP = static_cast<uint8_t>(GPR::Fp);
constexpr uint8_t X16 = 16;  // spill / address scratch (reserved, not allocatable)
constexpr size_t MAX_REG_ARGS = 8;
constexpr int IMM12_MAX = 4095;

auto IsFloatVReg(TargetOperand *op) -> bool {
  return op->IsVirtualReg() && static_cast<TargetVirtualReg *>(op)->GetRegClass() == TargetRegClass::Float;
}
auto IsWideVReg(TargetOperand *op) -> bool {
  return op->IsVirtualReg() && static_cast<TargetVirtualReg *>(op)->GetRegSize() == TargetRegSize::QWord;
}
auto VRegSize(TargetOperand *op) -> TargetRegSize {
  return op->IsVirtualReg() ? static_cast<TargetVirtualReg *>(op)->GetRegSize() : TargetRegSize::QWord;
}

// AArch64 condition for a signed comparison predicate (SysY is signed, NaN-free).
auto CondFor(backend::inst::CmpPredicate pred) -> A64Cond {
  using P = backend::inst::CmpPredicate;
  switch (pred) {
    case P::EQ: return A64Cond::EQ;
    case P::NE: return A64Cond::NE;
    case P::LT: return A64Cond::LT;
    case P::GT: return A64Cond::GT;
    case P::GE: return A64Cond::GE;
    case P::LE: return A64Cond::LE;
  }
  return A64Cond::EQ;
}

auto IsMemIntrinsic(const std::string &callee) -> bool {
  return callee.rfind("llvm.memset", 0) == 0 || callee.rfind("llvm.memcpy", 0) == 0;
}
auto IsMemsetIntrinsic(const std::string &callee) -> bool { return callee.rfind("llvm.memset", 0) == 0; }

auto CallArgCount(backend::inst::Instruction *inst) -> size_t {
  const auto &callee = static_cast<backend::inst::CallInst *>(inst)->GetCallee();
  if (IsMemIntrinsic(callee)) {
    return 3;
  }
  return inst->GetOperands().size();
}
}  // namespace

auto AArch64ISel::SelectFunction(backend::utils::Function *mir_func) -> TargetFunction * {
  func_name_ = mir_func->GetName();
  value_map_.clear();
  block_by_id_.clear();
  fused_icmps_.clear();
  use_count_.clear();
  one_user_.clear();
  alloca_offset_.clear();
  alloca_bytes_ = 0;
  outgoing_bytes_ = 0;

  size_t max_stack_args = 0;
  for (auto *mir_block : mir_func->GetBlocks()) {
    for (auto *inst : mir_block->GetInstructions()) {
      if (inst->GetOpKind() == OpKind::Call) {
        auto args = CallArgCount(inst);
        max_stack_args = std::max(max_stack_args, args > MAX_REG_ARGS ? args - MAX_REG_ARGS : 0);
      }
    }
  }
  outgoing_bytes_ = max_stack_args * 8;

  auto *tfunc = ctx_->NewFunction(func_name_);
  for (auto *mir_block : mir_func->GetBlocks()) {
    auto *tblock = ctx_->NewBlock(mir_block->GetId());
    tblock->SetLabel(LabelForBlock(mir_block->GetId())->GetName());
    block_by_id_[mir_block->GetId()] = tblock;
    tfunc->AddBlock(tblock);
  }

  for (auto *mir_block : mir_func->GetBlocks()) {
    for (auto *inst : mir_block->GetInstructions()) {
      for (auto *op : inst->GetOperands()) {
        if (op->IsRegister()) {
          use_count_[op]++;
          one_user_[op] = inst;
        }
      }
    }
  }
  for (auto *mir_block : mir_func->GetBlocks()) {
    for (auto *inst : mir_block->GetInstructions()) {
      if (inst->GetOpKind() == OpKind::ICmp && IsFusableToBranch(inst)) {
        fused_icmps_.insert(inst);
      }
    }
  }

  const auto &blocks = mir_func->GetBlocks();
  if (!blocks.empty()) {
    cur_block_ = block_by_id_[blocks.front()->GetId()];
    auto *func_def = compiler_context_->GetMIRContext()->GetFuncDefByName(func_name_);
    if (func_def != nullptr) {
      size_t int_idx = 0;
      size_t float_idx = 0;
      size_t stack_slot = 0;
      for (auto *param : func_def->GetOperands()) {
        auto *vreg = GetOrCreateVReg(param);
        if (param->IsFloat()) {
          if (float_idx < MAX_REG_ARGS) {
            Emit(A64Opcode::FMOV, vreg, {PhysFPR(static_cast<uint8_t>(FPR::V0) + static_cast<uint8_t>(float_idx))});
            float_idx++;
          } else {
            EmitBaseLoad(A64Opcode::LDR_OFF, vreg, FP, static_cast<int>(stack_slot * 8));
            stack_slot++;
          }
        } else if (int_idx < MAX_REG_ARGS) {
          Emit(A64Opcode::MOV, vreg, {PhysGPR(X0 + static_cast<uint8_t>(int_idx), VRegSize(vreg))});
          int_idx++;
        } else {
          EmitBaseLoad(A64Opcode::LDR_OFF, vreg, FP, static_cast<int>(stack_slot * 8));
          stack_slot++;
        }
      }
    }
  }

  for (auto *mir_block : mir_func->GetBlocks()) {
    cur_block_ = block_by_id_[mir_block->GetId()];
    for (auto *inst : mir_block->GetInstructions()) {
      SelectInst(inst);
    }
  }
  return tfunc;
}

auto AArch64ISel::SelectInst(backend::inst::Instruction *inst) -> void {
  switch (inst->GetOpKind()) {
    case OpKind::Add: SelectBinary(inst, A64Opcode::ADD); break;
    case OpKind::Sub: SelectBinary(inst, A64Opcode::SUB); break;
    case OpKind::Mul: SelectBinary(inst, A64Opcode::MUL); break;
    case OpKind::Div: SelectDiv(inst); break;
    case OpKind::Mod: SelectMod(inst); break;
    case OpKind::And: SelectBinary(inst, A64Opcode::AND); break;
    case OpKind::Or: SelectBinary(inst, A64Opcode::ORR); break;
    case OpKind::Xor: SelectBinary(inst, A64Opcode::EOR); break;
    case OpKind::Shl: SelectBinary(inst, A64Opcode::LSLV); break;
    case OpKind::LShr: SelectBinary(inst, A64Opcode::LSRV); break;
    case OpKind::AShr: SelectBinary(inst, A64Opcode::ASRV); break;
    case OpKind::AddF: SelectBinary(inst, A64Opcode::FADD); break;
    case OpKind::SubF: SelectBinary(inst, A64Opcode::FSUB); break;
    case OpKind::MulF: SelectBinary(inst, A64Opcode::FMUL); break;
    case OpKind::DivF: SelectBinary(inst, A64Opcode::FDIV); break;
    case OpKind::FCmp: SelectFCmp(inst); break;
    case OpKind::Int2Float: SelectConvert(inst, A64Opcode::SCVTF); break;
    case OpKind::Float2Int: SelectConvert(inst, A64Opcode::FCVTZS); break;
    case OpKind::Bool2Float: SelectConvert(inst, A64Opcode::SCVTF); break;
    case OpKind::ICmp: SelectICmp(inst); break;
    case OpKind::CondBr: SelectCondBr(inst); break;
    case OpKind::Br: SelectBr(inst); break;
    case OpKind::Ret: SelectRet(inst); break;
    case OpKind::Copy: SelectCopy(inst); break;
    case OpKind::Zext: SelectCopy(inst); break;
    case OpKind::Call: SelectCall(inst); break;
    case OpKind::Load: SelectLoad(inst); break;
    case OpKind::Store: SelectStore(inst); break;
    default:
      Comment("TODO select opkind " + std::to_string(static_cast<int>(inst->GetOpKind())));
      break;
  }
}

auto AArch64ISel::SelectBinary(backend::inst::Instruction *inst, A64Opcode opcode) -> void {
  auto *lhs = GetReg(inst->GetOperands()[0]);
  auto *rhs = GetReg(inst->GetOperands()[1]);
  Emit(opcode, GetOrCreateVReg(inst->GetResult()), {lhs, rhs});
}

auto AArch64ISel::SelectDiv(backend::inst::Instruction *inst) -> void {
  auto *lit = dynamic_cast<backend::utils::IntLiteral *>(inst->GetOperands()[1]);
  if (lit != nullptr) {
    auto *numer = GetReg(inst->GetOperands()[0]);
    if (auto *quot = EmitConstDivide(numer, lit->GetValue())) {
      Emit(A64Opcode::MOV, GetOrCreateVReg(inst->GetResult()), {quot});
      return;
    }
  }
  SelectBinary(inst, A64Opcode::SDIV);
}

auto AArch64ISel::SelectMod(backend::inst::Instruction *inst) -> void {
  // AArch64 has no modulo: r = a - (a / b) * b  via  sdiv q, a, b; msub r, q, b, a.
  auto *a = GetReg(inst->GetOperands()[0]);
  auto *lit = dynamic_cast<backend::utils::IntLiteral *>(inst->GetOperands()[1]);
  if (lit != nullptr) {
    if (auto *q = EmitConstDivide(a, lit->GetValue())) {
      auto *d = ctx_->NewVirtualReg(TargetRegClass::General, TargetRegSize::DWord);
      EmitMovImm(d, lit->GetValue());
      Emit(A64Opcode::MSUB, GetOrCreateVReg(inst->GetResult()), {q, d, a});
      return;
    }
  }
  auto *b = GetReg(inst->GetOperands()[1]);
  auto *q = ctx_->NewVirtualReg(TargetRegClass::General, TargetRegSize::DWord);
  Emit(A64Opcode::SDIV, q, {a, b});
  Emit(A64Opcode::MSUB, GetOrCreateVReg(inst->GetResult()), {q, b, a});
}

// Signed division of `numer` by the compile-time constant `divisor`, avoiding the
// costly `sdiv`. Positive powers of two use a sign-biased shift; other divisors use
// a magic multiply-high + shift (Hacker's Delight). Returns the quotient register,
// or nullptr for divisors best left to `sdiv` (|d| < 2, d == INT_MIN, which magic
// cannot represent). All arithmetic is on 32-bit (w) registers.
auto AArch64ISel::EmitConstDivide(TargetOperand *numer, int divisor) -> TargetOperand * {
  auto new_reg = [&] { return ctx_->NewVirtualReg(TargetRegClass::General, TargetRegSize::DWord); };
  if (!magic_div_ || divisor == -1 || divisor == INT32_MIN) {
    return nullptr;  // -x needs a zero reg; INT_MIN breaks the magic algorithm
  }
  if (divisor == 1) {
    return numer;
  }
  unsigned ad = divisor < 0 ? 0U - static_cast<unsigned>(divisor) : static_cast<unsigned>(divisor);

  if (divisor > 0 && (ad & (ad - 1)) == 0) {
    // q = (n + (n >> 31 logical-masked to 2^k-1 when negative)) >> k.
    int k = __builtin_ctz(ad);
    auto *sign = new_reg();
    Emit(A64Opcode::ASR_I, sign, {numer, ctx_->Imm(31)});        // 0 or -1
    auto *bias = new_reg();
    Emit(A64Opcode::LSR_I, bias, {sign, ctx_->Imm(32 - k)});     // 0 or (2^k - 1)
    auto *adj = new_reg();
    Emit(A64Opcode::ADD, adj, {numer, bias});
    auto *quot = new_reg();
    Emit(A64Opcode::ASR_I, quot, {adj, ctx_->Imm(k)});
    return quot;
  }

  auto m = ComputeSignedDivMagic(divisor);
  auto *magic = new_reg();
  EmitMovImm(magic, m.magic_);
  auto *q = new_reg();
  Emit(A64Opcode::SMULH, q, {numer, magic});                    // high 32 of n * magic
  if (divisor > 0 && m.magic_ < 0) {
    auto *t = new_reg();
    Emit(A64Opcode::ADD, t, {q, numer});
    q = t;
  } else if (divisor < 0 && m.magic_ > 0) {
    auto *t = new_reg();
    Emit(A64Opcode::SUB, t, {q, numer});
    q = t;
  }
  if (m.shift_ > 0) {
    auto *t = new_reg();
    Emit(A64Opcode::ASR_I, t, {q, ctx_->Imm(m.shift_)});
    q = t;
  }
  // Add the sign bit of the shifted quotient to round toward zero.
  auto *sign = new_reg();
  Emit(A64Opcode::LSR_I, sign, {q, ctx_->Imm(31)});
  auto *quot = new_reg();
  Emit(A64Opcode::ADD, quot, {q, sign});
  return quot;
}

auto AArch64ISel::SelectICmp(backend::inst::Instruction *inst) -> void {
  if (fused_icmps_.count(inst) != 0U) {
    return;  // folded into the consuming branch
  }
  auto pred = static_cast<backend::inst::ICmpInst *>(inst)->GetPredicate();
  auto *lhs = GetReg(inst->GetOperands()[0]);
  auto *rhs = GetReg(inst->GetOperands()[1]);
  Emit(A64Opcode::CMP, nullptr, {lhs, rhs});
  EmitCond(A64Opcode::CSET, GetOrCreateVReg(inst->GetResult()), CondFor(pred), {});
}

auto AArch64ISel::SelectFCmp(backend::inst::Instruction *inst) -> void {
  auto pred = static_cast<backend::inst::FCmpInst *>(inst)->GetPredicate();
  auto *lhs = GetReg(inst->GetOperands()[0]);
  auto *rhs = GetReg(inst->GetOperands()[1]);
  Emit(A64Opcode::FCMP, nullptr, {lhs, rhs});
  EmitCond(A64Opcode::CSET, GetOrCreateVReg(inst->GetResult()), CondFor(pred), {});
}

auto AArch64ISel::SelectConvert(backend::inst::Instruction *inst, A64Opcode opcode) -> void {
  auto *src = GetReg(inst->GetOperands()[0]);
  Emit(opcode, GetOrCreateVReg(inst->GetResult()), {src});
}

auto AArch64ISel::SelectCondBr(backend::inst::Instruction *inst) -> void {
  const auto &ops = inst->GetOperands();
  auto *cond = ops[0];
  auto true_id = static_cast<backend::utils::LabelOperand *>(ops[1])->GetId();
  auto false_id = static_cast<backend::utils::LabelOperand *>(ops[2])->GetId();

  auto *def = cond->GetDefInst();
  if (def != nullptr && fused_icmps_.count(def) != 0U) {
    auto pred = static_cast<backend::inst::ICmpInst *>(def)->GetPredicate();
    auto *lhs = GetReg(def->GetOperands()[0]);
    auto *rhs = GetReg(def->GetOperands()[1]);
    Emit(A64Opcode::CMP, nullptr, {lhs, rhs});
    EmitCond(A64Opcode::B_COND, nullptr, CondFor(pred), {LabelForBlock(true_id)});
  } else {
    auto *creg = GetReg(cond);
    Emit(A64Opcode::CBNZ, nullptr, {creg, LabelForBlock(true_id)});
  }
  Emit(A64Opcode::B, nullptr, {LabelForBlock(false_id)});
}

auto AArch64ISel::SelectBr(backend::inst::Instruction *inst) -> void {
  auto target_id = static_cast<backend::utils::LabelOperand *>(inst->GetOperands()[0])->GetId();
  Emit(A64Opcode::B, nullptr, {LabelForBlock(target_id)});
}

auto AArch64ISel::SelectRet(backend::inst::Instruction *inst) -> void {
  const auto &ops = inst->GetOperands();
  if (!ops.empty()) {
    if (ops[0]->IsFloat()) {
      Emit(A64Opcode::FMOV, PhysFPR(static_cast<uint8_t>(FPR::V0)), {GetReg(ops[0])});
    } else {
      auto *val = GetReg(ops[0]);
      Emit(A64Opcode::MOV, PhysGPR(X0, VRegSize(val)), {val});
    }
  }
  Emit(A64Opcode::RET, nullptr, {});
}

auto AArch64ISel::SelectCopy(backend::inst::Instruction *inst) -> void {
  auto *src = GetReg(inst->GetOperands()[0]);
  auto opcode = inst->GetResult()->IsFloat() ? A64Opcode::FMOV : A64Opcode::MOV;
  Emit(opcode, GetOrCreateVReg(inst->GetResult()), {src});
}

auto AArch64ISel::SelectCall(backend::inst::Instruction *inst) -> void {
  auto *call = static_cast<backend::inst::CallInst *>(inst);
  const auto &callee = call->GetCallee();

  if (IsMemIntrinsic(callee)) {
    bool is_memset = IsMemsetIntrinsic(callee);
    const auto &ops = inst->GetOperands();
    Emit(A64Opcode::MOV, PhysGPR(X0, TargetRegSize::QWord), {LowerAddress(ops[0])});
    Emit(A64Opcode::MOV, PhysGPR(X0 + 1, is_memset ? TargetRegSize::DWord : TargetRegSize::QWord),
         {is_memset ? GetReg(ops[1]) : LowerAddress(ops[1])});
    Emit(A64Opcode::MOV, PhysGPR(X0 + 2, TargetRegSize::DWord), {GetReg(ops[2])});
    Emit(A64Opcode::BL, nullptr,
         {ctx_->Symbol(is_memset ? "memset" : "memcpy"), PhysGPR(X0, TargetRegSize::QWord),
          PhysGPR(X0 + 1, TargetRegSize::QWord), PhysGPR(X0 + 2, TargetRegSize::QWord)});
    return;
  }

  size_t int_idx = 0;
  size_t float_idx = 0;
  size_t stack_slot = 0;
  std::vector<TargetOperand *> call_ops{ctx_->Symbol(call->GetCallee())};
  for (auto *param : inst->GetOperands()) {
    auto *val = GetReg(param);
    if (IsFloatVReg(val)) {
      if (float_idx < MAX_REG_ARGS) {
        auto *va = PhysFPR(static_cast<uint8_t>(FPR::V0) + static_cast<uint8_t>(float_idx));
        Emit(A64Opcode::FMOV, va, {val});
        call_ops.push_back(va);
        float_idx++;
      } else {
        EmitBaseStore(A64Opcode::STR_OFF, val, SP, static_cast<int>(stack_slot * 8));
        stack_slot++;
      }
    } else if (int_idx < MAX_REG_ARGS) {
      auto *areg = PhysGPR(X0 + static_cast<uint8_t>(int_idx), VRegSize(val));
      Emit(A64Opcode::MOV, areg, {val});
      call_ops.push_back(areg);
      int_idx++;
    } else {
      EmitBaseStore(A64Opcode::STR_OFF, val, SP, static_cast<int>(stack_slot * 8));
      stack_slot++;
    }
  }
  Emit(A64Opcode::BL, nullptr, call_ops);
  if (inst->GetResult() != nullptr) {
    if (inst->GetResult()->IsFloat()) {
      Emit(A64Opcode::FMOV, GetOrCreateVReg(inst->GetResult()), {PhysFPR(static_cast<uint8_t>(FPR::V0))});
    } else {
      auto *res = GetOrCreateVReg(inst->GetResult());
      Emit(A64Opcode::MOV, res, {PhysGPR(X0, VRegSize(res))});
    }
  }
}

auto AArch64ISel::GetOrCreateVReg(backend::utils::Operand *mir_operand) -> TargetOperand * {
  auto it = value_map_.find(mir_operand);
  if (it != value_map_.end()) {
    return it->second;
  }
  auto reg_class = mir_operand->IsFloat() ? TargetRegClass::Float : TargetRegClass::General;
  auto reg_size = mir_operand->IsPtr() ? TargetRegSize::QWord : TargetRegSize::DWord;
  auto *vreg = ctx_->NewVirtualReg(reg_class, reg_size);
  value_map_[mir_operand] = vreg;
  return vreg;
}

auto AArch64ISel::GetReg(backend::utils::Operand *mir_operand) -> TargetOperand * {
  if (mir_operand->IsRegister()) {
    return GetOrCreateVReg(mir_operand);
  }
  if (mir_operand->IsLiteral()) {
    if (auto *flit = dynamic_cast<backend::utils::FloatLiteral *>(mir_operand)) {
      float value = flit->GetValue();
      int bits = 0;
      std::memcpy(&bits, &value, sizeof(bits));
      auto *greg = ctx_->NewVirtualReg(TargetRegClass::General, TargetRegSize::DWord);
      EmitMovImm(greg, bits);
      auto *freg = ctx_->NewVirtualReg(TargetRegClass::Float, TargetRegSize::DWord);
      Emit(A64Opcode::FMOV_FROM_GPR, freg, {greg});
      return freg;
    }
    int value = 0;
    if (auto *lit = dynamic_cast<backend::utils::IntLiteral *>(mir_operand)) {
      value = lit->GetValue();
    } else if (auto *lit = dynamic_cast<backend::utils::BoolLiteral *>(mir_operand)) {
      value = lit->GetValue() ? 1 : 0;
    } else if (auto *lit = dynamic_cast<backend::utils::CharLiteral *>(mir_operand)) {
      value = static_cast<int>(lit->GetValue());
    } else if (auto *lit = dynamic_cast<backend::utils::LongLiteral *>(mir_operand)) {
      value = static_cast<int>(lit->GetValue());
    } else {
      Comment("TODO: unhandled literal kind");
    }
    auto *vreg = ctx_->NewVirtualReg(TargetRegClass::General, TargetRegSize::DWord);
    EmitMovImm(vreg, value);
    return vreg;
  }
  if (mir_operand->IsFrame() || mir_operand->IsGlobal() || mir_operand->IsMemory()) {
    return LowerAddress(mir_operand);
  }
  Comment("TODO: unhandled operand kind");
  return PhysGPR(X0, TargetRegSize::QWord);
}

auto AArch64ISel::PhysGPR(uint8_t id, TargetRegSize size) -> TargetOperand * {
  return ctx_->PhysReg(TargetRegClass::General, size, id);
}
auto AArch64ISel::PhysFPR(uint8_t id) -> TargetOperand * {
  return ctx_->PhysReg(TargetRegClass::Float, TargetRegSize::DWord, id);
}
auto AArch64ISel::LabelForBlock(size_t block_id) -> TargetLabel * {
  return ctx_->Label(".L_" + func_name_ + "_" + std::to_string(block_id));
}

auto AArch64ISel::Emit(A64Opcode opcode, TargetOperand *result, const std::vector<TargetOperand *> &operands)
    -> AArch64Inst * {
  auto *inst = ctx_->CreateInst<AArch64Inst>(opcode);
  if (result != nullptr) {
    inst->SetResult(result);
  }
  for (auto *op : operands) {
    inst->AddOperand(op);
  }
  cur_block_->AddInst(inst);
  return inst;
}

auto AArch64ISel::EmitCond(A64Opcode opcode, TargetOperand *result, A64Cond cond,
                           const std::vector<TargetOperand *> &operands) -> AArch64Inst * {
  auto *inst = Emit(opcode, result, operands);
  inst->SetCond(cond);
  return inst;
}

auto AArch64ISel::Comment(const std::string &text) -> void {
  auto *inst = ctx_->CreateInst<AArch64Inst>(A64Opcode::COMMENT);
  inst->SetComment(text);
  cur_block_->AddInst(inst);
}

auto AArch64ISel::EmitMovImm(TargetOperand *dst, int value) -> void {
  auto v = static_cast<uint32_t>(value);
  uint32_t lo = v & 0xFFFFU;
  uint32_t hi = (v >> 16) & 0xFFFFU;
  if (hi == 0) {
    Emit(A64Opcode::MOVZ, dst, {ctx_->Imm(static_cast<int>(lo))});
  } else if (hi == 0xFFFFU) {
    Emit(A64Opcode::MOVN, dst, {ctx_->Imm(static_cast<int>((~lo) & 0xFFFFU))});
  } else {
    Emit(A64Opcode::MOVZ, dst, {ctx_->Imm(static_cast<int>(lo))});
    Emit(A64Opcode::MOVK, dst, {dst, ctx_->Imm(static_cast<int>(hi)), ctx_->Imm(16)});
  }
}

auto AArch64ISel::EmitBaseLoad(A64Opcode opcode, TargetOperand *dst, uint8_t base_id, int offset) -> void {
  if (offset >= 0 && offset <= IMM12_MAX) {
    Emit(opcode, dst, {PhysGPR(base_id, TargetRegSize::QWord), ctx_->Imm(offset)});
    return;
  }
  auto *addr = ctx_->NewVirtualReg(TargetRegClass::General, TargetRegSize::QWord);
  EmitMovImm(addr, offset);
  Emit(A64Opcode::ADD, addr, {PhysGPR(base_id, TargetRegSize::QWord), addr});
  Emit(opcode, dst, {addr, ctx_->Imm(0)});
}

auto AArch64ISel::EmitBaseStore(A64Opcode opcode, TargetOperand *value, uint8_t base_id, int offset) -> void {
  if (offset >= 0 && offset <= IMM12_MAX) {
    Emit(opcode, nullptr, {value, PhysGPR(base_id, TargetRegSize::QWord), ctx_->Imm(offset)});
    return;
  }
  auto *addr = ctx_->NewVirtualReg(TargetRegClass::General, TargetRegSize::QWord);
  EmitMovImm(addr, offset);
  Emit(A64Opcode::ADD, addr, {PhysGPR(base_id, TargetRegSize::QWord), addr});
  Emit(opcode, nullptr, {value, addr, ctx_->Imm(0)});
}

auto AArch64ISel::SelectLoad(backend::inst::Instruction *inst) -> void {
  auto *addr = LowerAddress(inst->GetOperands()[0]);
  auto *res = GetOrCreateVReg(inst->GetResult());
  Emit(A64Opcode::LDR_OFF, res, {addr, ctx_->Imm(0)});
}

auto AArch64ISel::SelectStore(backend::inst::Instruction *inst) -> void {
  auto *value = GetReg(inst->GetOperands()[0]);
  auto *addr = LowerAddress(inst->GetOperands()[1]);
  Emit(A64Opcode::STR_OFF, nullptr, {value, addr, ctx_->Imm(0)});
}

auto AArch64ISel::AllocaOffset(backend::utils::Operand *frame_op) -> size_t {
  auto it = alloca_offset_.find(frame_op);
  if (it != alloca_offset_.end()) {
    return it->second;
  }
  auto *frame_operand = static_cast<backend::utils::FrameOperand *>(frame_op);
  auto *info = compiler_context_->GetMIRContext()->GetLowerContext()->GetFrameInfoFromFrameOperand(frame_operand);
  auto size = info->GetSize();
  auto offset = outgoing_bytes_ + alloca_bytes_;
  alloca_offset_[frame_op] = offset;
  alloca_bytes_ += (size + 7U) & ~static_cast<size_t>(7U);
  return offset;
}

auto AArch64ISel::LowerAddress(backend::utils::Operand *ptr) -> TargetOperand * {
  if (ptr->IsFrame()) {
    auto offset = static_cast<int>(AllocaOffset(ptr));
    auto *addr = ctx_->NewVirtualReg(TargetRegClass::General, TargetRegSize::QWord);
    auto *sp = PhysGPR(SP, TargetRegSize::QWord);
    if (offset >= 0 && offset <= IMM12_MAX) {
      Emit(A64Opcode::ADD_I, addr, {sp, ctx_->Imm(offset)});
    } else {
      EmitMovImm(addr, offset);
      Emit(A64Opcode::ADD, addr, {sp, addr});
    }
    return addr;
  }
  if (ptr->IsGlobal()) {
    auto *addr = ctx_->NewVirtualReg(TargetRegClass::General, TargetRegSize::QWord);
    auto *sym = ctx_->Symbol(static_cast<backend::utils::GlobalOperand *>(ptr)->GetName());
    Emit(A64Opcode::ADRP, addr, {sym});
    Emit(A64Opcode::ADD_LO12, addr, {addr, sym});
    return addr;
  }
  if (ptr->IsRegister()) {
    return GetOrCreateVReg(ptr);
  }
  if (ptr->IsMemory()) {
    auto *mem = static_cast<backend::utils::MemoryOperand *>(ptr);
    const auto &info = mem->GetAddressInfo();
    if (info.base_ptr_ == nullptr) {
      Comment("TODO: memory operand without base pointer");
      return PhysGPR(X0, TargetRegSize::QWord);
    }
    auto *addr = LowerAddress(info.base_ptr_);
    const auto &dims = info.dimensions_;
    const auto &indices = info.indices_;
    constexpr int ELEM = 4;

    auto m = indices.size();
    auto n = dims.size();
    if (m == 0) {
      return addr;
    }
    bool decayed = !(n > 0 && dims[0] < 0);
    std::vector<long> strides(m, ELEM);
    for (size_t k = 0; k < m; ++k) {
      long stride = ELEM;
      size_t start_d = decayed ? k : k + 1;
      for (size_t d = start_d; d < n; ++d) {
        stride *= dims[d];
      }
      strides[k] = stride;
    }

    for (size_t k = 0; k < m; ++k) {
      auto *lit = dynamic_cast<backend::utils::IntLiteral *>(indices[k]);
      if (lit != nullptr && lit->GetValue() == 0) {
        continue;
      }
      // Sign-extend the 32-bit index to 64 bits, scale by the stride, add to base.
      auto *idx32 = GetReg(indices[k]);
      auto *idx64 = ctx_->NewVirtualReg(TargetRegClass::General, TargetRegSize::QWord);
      Emit(A64Opcode::SXTW, idx64, {idx32});
      auto *stride_reg = ctx_->NewVirtualReg(TargetRegClass::General, TargetRegSize::QWord);
      EmitMovImm(stride_reg, static_cast<int>(strides[k]));
      auto *term = ctx_->NewVirtualReg(TargetRegClass::General, TargetRegSize::QWord);
      Emit(A64Opcode::MUL, term, {idx64, stride_reg});
      auto *next = ctx_->NewVirtualReg(TargetRegClass::General, TargetRegSize::QWord);
      Emit(A64Opcode::ADD, next, {addr, term});
      addr = next;
    }
    return addr;
  }
  Comment("TODO: unhandled address kind");
  return PhysGPR(X0, TargetRegSize::QWord);
}

auto AArch64ISel::IsFusableToBranch(backend::inst::Instruction *icmp) const -> bool {
  auto *res = icmp->GetResult();
  if (res == nullptr) {
    return false;
  }
  auto count_it = use_count_.find(res);
  if (count_it == use_count_.end() || count_it->second != 1) {
    return false;
  }
  auto user_it = one_user_.find(res);
  return user_it != one_user_.end() && user_it->second->GetOpKind() == OpKind::CondBr;
}

}  // namespace backend::targetir::aarch64
