#include "backend/targets/riscv/isel/riscvisel.h"
#include <algorithm>
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
#include "backend/targets/riscv/regs/riscvreg.h"
#include "context/compiler_context.h"

namespace backend::targetir::riscv {

using backend::inst::OpKind;

namespace {
constexpr auto A0 = static_cast<uint8_t>(GPR::A0);
constexpr auto ZERO = static_cast<uint8_t>(GPR::Zero);
constexpr auto SP = static_cast<uint8_t>(GPR::Sp);
constexpr auto FP = static_cast<uint8_t>(GPR::Fp);
constexpr size_t MAX_REG_ARGS = 8;

// True if a selected value lives in a floating-point register. The vreg class is
// reliable where the MIR operand type is not (e.g. untyped float literals).
auto IsFloatVReg(TargetOperand *op) -> bool {
  return op->IsVirtualReg() && static_cast<TargetVirtualReg *>(op)->GetRegClass() == TargetRegClass::Float;
}

auto IsWideVReg(TargetOperand *op) -> bool {
  return op->IsVirtualReg() && static_cast<TargetVirtualReg *>(op)->GetRegSize() == TargetRegSize::QWord;
}

// Number of arguments a call actually passes through the integer ABI. The
// memset/memcpy intrinsics are rewritten to 3-argument libc calls.
auto CallArgCount(backend::inst::Instruction *inst) -> size_t {
  const auto &callee = static_cast<backend::inst::CallInst *>(inst)->GetCallee();
  if (callee.find("memset") != std::string::npos || callee.find("memcpy") != std::string::npos) {
    return 3;
  }
  return inst->GetOperands().size();
}
}  // namespace

auto RISCVISel::SelectFunction(backend::utils::Function *mir_func) -> TargetFunction * {
  func_name_ = mir_func->GetName();
  value_map_.clear();
  block_by_id_.clear();
  fused_icmps_.clear();
  use_count_.clear();
  one_user_.clear();
  alloca_offset_.clear();
  alloca_bytes_ = 0;
  outgoing_bytes_ = 0;

  // Pre-scan: size the outgoing stack-argument area from the widest call, so the
  // alloca area can sit just above it (offsets are then known before selection).
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

  // Pass 1: create a target block per MIR block, keyed by MIR block id.
  for (auto *mir_block : mir_func->GetBlocks()) {
    auto *tblock = ctx_->NewBlock(mir_block->GetId());
    tblock->SetLabel(LabelForBlock(mir_block->GetId())->GetName());
    block_by_id_[mir_block->GetId()] = tblock;
    tfunc->AddBlock(tblock);
  }

  // Def-use summary + compare/branch fusion candidates.
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

  // Entry block: bring incoming register arguments in from a0..a7.
  const auto &blocks = mir_func->GetBlocks();
  if (!blocks.empty()) {
    cur_block_ = block_by_id_[blocks.front()->GetId()];
    auto *func_def = compiler_context_->GetMIRContext()->GetFuncDefByName(func_name_);
    if (func_def != nullptr) {
      // Integer args come in a0-a7, float args in fa0-fa7 (counted separately);
      // overflow goes to the stack in argument order, read via fp (incoming sp).
      size_t int_idx = 0;
      size_t float_idx = 0;
      size_t stack_slot = 0;
      for (auto *param : func_def->GetOperands()) {
        auto *vreg = GetOrCreateVReg(param);
        if (param->IsFloat()) {
          if (float_idx < MAX_REG_ARGS) {
            Emit(RVOpcode::FMV_S, vreg, {PhysFPR(static_cast<uint8_t>(FPR::Fa0) + static_cast<uint8_t>(float_idx))});
            float_idx++;
          } else {
            Emit(RVOpcode::FLW, vreg, {PhysGPR(FP), ctx_->Imm(static_cast<int>(stack_slot * 8))});
            stack_slot++;
          }
        } else if (int_idx < MAX_REG_ARGS) {
          Emit(RVOpcode::MV, vreg, {PhysGPR(A0 + static_cast<uint8_t>(int_idx))});
          int_idx++;
        } else {
          auto opcode = param->IsPtr() ? RVOpcode::LD : RVOpcode::LW;
          Emit(opcode, vreg, {PhysGPR(FP), ctx_->Imm(static_cast<int>(stack_slot * 8))});
          stack_slot++;
        }
      }
    }
  }

  // Pass 2: select each block's instructions.
  for (auto *mir_block : mir_func->GetBlocks()) {
    cur_block_ = block_by_id_[mir_block->GetId()];
    for (auto *inst : mir_block->GetInstructions()) {
      SelectInst(inst);
    }
  }
  return tfunc;
}

auto RISCVISel::SelectInst(backend::inst::Instruction *inst) -> void {
  switch (inst->GetOpKind()) {
    case OpKind::Add: SelectBinary(inst, RVOpcode::ADDW); break;
    case OpKind::Sub: SelectBinary(inst, RVOpcode::SUBW); break;
    case OpKind::Mul: SelectBinary(inst, RVOpcode::MULW); break;
    case OpKind::Div: SelectBinary(inst, RVOpcode::DIVW); break;
    case OpKind::Mod: SelectBinary(inst, RVOpcode::REMW); break;
    case OpKind::And: SelectBinary(inst, RVOpcode::AND); break;
    case OpKind::Or: SelectBinary(inst, RVOpcode::OR); break;
    case OpKind::Xor: SelectBinary(inst, RVOpcode::XOR); break;
    case OpKind::Shl: SelectBinary(inst, RVOpcode::SLLW); break;
    case OpKind::LShr: SelectBinary(inst, RVOpcode::SRLW); break;
    case OpKind::AShr: SelectBinary(inst, RVOpcode::SRAW); break;
    case OpKind::AddF: SelectBinary(inst, RVOpcode::FADD_S); break;
    case OpKind::SubF: SelectBinary(inst, RVOpcode::FSUB_S); break;
    case OpKind::MulF: SelectBinary(inst, RVOpcode::FMUL_S); break;
    case OpKind::DivF: SelectBinary(inst, RVOpcode::FDIV_S); break;
    case OpKind::FCmp: SelectFCmp(inst); break;
    case OpKind::Int2Float: SelectConvert(inst, RVOpcode::FCVT_S_W); break;
    case OpKind::Float2Int: SelectConvert(inst, RVOpcode::FCVT_W_S); break;
    case OpKind::Bool2Float: SelectConvert(inst, RVOpcode::FCVT_S_W); break;
    case OpKind::ICmp: SelectICmp(inst); break;
    case OpKind::CondBr: SelectCondBr(inst); break;
    case OpKind::Br: SelectBr(inst); break;
    case OpKind::Ret: SelectRet(inst); break;
    case OpKind::Copy: SelectCopy(inst); break;
    case OpKind::Zext: SelectCopy(inst); break;  // i1 -> i32 is a value-preserving move
    case OpKind::Call: SelectCall(inst); break;
    case OpKind::Load: SelectLoad(inst); break;
    case OpKind::Store: SelectStore(inst); break;
    default:
      Comment("TODO select opkind " + std::to_string(static_cast<int>(inst->GetOpKind())));
      break;
  }
}

auto RISCVISel::SelectBinary(backend::inst::Instruction *inst, RVOpcode opcode) -> void {
  auto *lhs = GetReg(inst->GetOperands()[0]);
  auto *rhs = GetReg(inst->GetOperands()[1]);
  Emit(opcode, GetOrCreateVReg(inst->GetResult()), {lhs, rhs});
}

auto RISCVISel::SelectICmp(backend::inst::Instruction *inst) -> void {
  if (fused_icmps_.count(inst) != 0U) {
    return;  // folded into the consuming branch
  }
  auto pred = static_cast<backend::inst::ICmpInst *>(inst)->GetPredicate();
  auto *lhs = GetReg(inst->GetOperands()[0]);
  auto *rhs = GetReg(inst->GetOperands()[1]);
  auto *res = GetOrCreateVReg(inst->GetResult());
  using backend::inst::CmpPredicate;
  switch (pred) {
    case CmpPredicate::LT: Emit(RVOpcode::SLT, res, {lhs, rhs}); break;
    case CmpPredicate::GT: Emit(RVOpcode::SLT, res, {rhs, lhs}); break;
    case CmpPredicate::GE:
      Emit(RVOpcode::SLT, res, {lhs, rhs});
      Emit(RVOpcode::XORI, res, {res, ctx_->Imm(1)});
      break;
    case CmpPredicate::LE:
      Emit(RVOpcode::SLT, res, {rhs, lhs});
      Emit(RVOpcode::XORI, res, {res, ctx_->Imm(1)});
      break;
    case CmpPredicate::EQ: {
      auto *diff = ctx_->NewVirtualReg(TargetRegClass::General, TargetRegSize::DWord);
      Emit(RVOpcode::SUBW, diff, {lhs, rhs});
      Emit(RVOpcode::SEQZ, res, {diff});
      break;
    }
    case CmpPredicate::NE: {
      auto *diff = ctx_->NewVirtualReg(TargetRegClass::General, TargetRegSize::DWord);
      Emit(RVOpcode::SUBW, diff, {lhs, rhs});
      Emit(RVOpcode::SNEZ, res, {diff});
      break;
    }
  }
}

auto RISCVISel::SelectFCmp(backend::inst::Instruction *inst) -> void {
  auto pred = static_cast<backend::inst::FCmpInst *>(inst)->GetPredicate();
  auto *lhs = GetReg(inst->GetOperands()[0]);
  auto *rhs = GetReg(inst->GetOperands()[1]);
  auto *res = GetOrCreateVReg(inst->GetResult());  // 0/1 in a GPR
  using backend::inst::CmpPredicate;
  switch (pred) {
    case CmpPredicate::EQ: Emit(RVOpcode::FEQ_S, res, {lhs, rhs}); break;
    case CmpPredicate::NE:
      Emit(RVOpcode::FEQ_S, res, {lhs, rhs});
      Emit(RVOpcode::XORI, res, {res, ctx_->Imm(1)});
      break;
    case CmpPredicate::LT: Emit(RVOpcode::FLT_S, res, {lhs, rhs}); break;
    case CmpPredicate::LE: Emit(RVOpcode::FLE_S, res, {lhs, rhs}); break;
    case CmpPredicate::GT: Emit(RVOpcode::FLT_S, res, {rhs, lhs}); break;
    case CmpPredicate::GE: Emit(RVOpcode::FLE_S, res, {rhs, lhs}); break;
  }
}

auto RISCVISel::SelectConvert(backend::inst::Instruction *inst, RVOpcode opcode) -> void {
  auto *src = GetReg(inst->GetOperands()[0]);
  Emit(opcode, GetOrCreateVReg(inst->GetResult()), {src});
}

auto RISCVISel::SelectCondBr(backend::inst::Instruction *inst) -> void {
  const auto &ops = inst->GetOperands();
  auto *cond = ops[0];
  auto true_id = static_cast<backend::utils::LabelOperand *>(ops[1])->GetId();
  auto false_id = static_cast<backend::utils::LabelOperand *>(ops[2])->GetId();

  auto *def = cond->GetDefInst();
  if (def != nullptr && fused_icmps_.count(def) != 0U) {
    auto pred = static_cast<backend::inst::ICmpInst *>(def)->GetPredicate();
    auto *lhs = GetReg(def->GetOperands()[0]);
    auto *rhs = GetReg(def->GetOperands()[1]);
    auto *true_label = LabelForBlock(true_id);
    using backend::inst::CmpPredicate;
    switch (pred) {
      case CmpPredicate::EQ: Emit(RVOpcode::BEQ, nullptr, {lhs, rhs, true_label}); break;
      case CmpPredicate::NE: Emit(RVOpcode::BNE, nullptr, {lhs, rhs, true_label}); break;
      case CmpPredicate::LT: Emit(RVOpcode::BLT, nullptr, {lhs, rhs, true_label}); break;
      case CmpPredicate::GE: Emit(RVOpcode::BGE, nullptr, {lhs, rhs, true_label}); break;
      case CmpPredicate::GT: Emit(RVOpcode::BLT, nullptr, {rhs, lhs, true_label}); break;
      case CmpPredicate::LE: Emit(RVOpcode::BGE, nullptr, {rhs, lhs, true_label}); break;
    }
  } else {
    auto *creg = GetReg(cond);
    Emit(RVOpcode::BNE, nullptr, {creg, Zero(), LabelForBlock(true_id)});
  }
  Emit(RVOpcode::J, nullptr, {LabelForBlock(false_id)});
}

auto RISCVISel::SelectBr(backend::inst::Instruction *inst) -> void {
  auto target_id = static_cast<backend::utils::LabelOperand *>(inst->GetOperands()[0])->GetId();
  Emit(RVOpcode::J, nullptr, {LabelForBlock(target_id)});
}

auto RISCVISel::SelectRet(backend::inst::Instruction *inst) -> void {
  const auto &ops = inst->GetOperands();
  if (!ops.empty()) {
    if (ops[0]->IsFloat()) {
      Emit(RVOpcode::FMV_S, PhysFPR(static_cast<uint8_t>(FPR::Fa0)), {GetReg(ops[0])});
    } else {
      Emit(RVOpcode::MV, PhysGPR(A0), {GetReg(ops[0])});
    }
  }
  // NOTE: prologue/epilogue (callee-saved restore, sp adjust) inserted by the
  // frame layer in M2; here we emit the bare `ret`.
  Emit(RVOpcode::RET, nullptr, {});
}

auto RISCVISel::SelectCopy(backend::inst::Instruction *inst) -> void {
  auto *src = GetReg(inst->GetOperands()[0]);
  auto opcode = inst->GetResult()->IsFloat() ? RVOpcode::FMV_S : RVOpcode::MV;
  Emit(opcode, GetOrCreateVReg(inst->GetResult()), {src});
}

auto RISCVISel::SelectCall(backend::inst::Instruction *inst) -> void {
  auto *call = static_cast<backend::inst::CallInst *>(inst);
  const auto &callee = call->GetCallee();

  // The LLVM memset/memcpy intrinsics lower to the libc routines, which sylib /
  // the C runtime provide. Their pointer arguments are addresses, not values.
  if (callee.find("memset") != std::string::npos || callee.find("memcpy") != std::string::npos) {
    bool is_memset = callee.find("memset") != std::string::npos;
    const auto &ops = inst->GetOperands();
    Emit(RVOpcode::MV, PhysGPR(A0), {LowerAddress(ops[0])});  // dest address
    // memset(dest, val, n); memcpy(dest, src, n) -- src is also an address.
    Emit(RVOpcode::MV, PhysGPR(A0 + 1), {is_memset ? GetReg(ops[1]) : LowerAddress(ops[1])});
    Emit(RVOpcode::MV, PhysGPR(A0 + 2), {GetReg(ops[2])});
    // The argument registers are recorded as CALL operands (after the symbol, and
    // ignored by the printer) so register allocation sees them live into the call.
    Emit(RVOpcode::CALL, nullptr,
         {ctx_->Symbol(is_memset ? "memset" : "memcpy"), PhysGPR(A0), PhysGPR(A0 + 1), PhysGPR(A0 + 2)});
    return;
  }

  size_t int_idx = 0;
  size_t float_idx = 0;
  size_t stack_slot = 0;
  // Argument registers, recorded as CALL operands so the allocator keeps them
  // live from their setup move to the call (they are ignored by the printer).
  std::vector<TargetOperand *> call_ops{ctx_->Symbol(call->GetCallee())};
  for (auto *param : inst->GetOperands()) {
    auto *val = GetReg(param);
    if (IsFloatVReg(val)) {
      if (float_idx < MAX_REG_ARGS) {
        auto *fa = PhysFPR(static_cast<uint8_t>(FPR::Fa0) + static_cast<uint8_t>(float_idx));
        Emit(RVOpcode::FMV_S, fa, {val});
        call_ops.push_back(fa);
        float_idx++;
      } else {
        Emit(RVOpcode::FSW, nullptr, {val, PhysGPR(SP), ctx_->Imm(static_cast<int>(stack_slot * 8))});
        stack_slot++;
      }
    } else if (int_idx < MAX_REG_ARGS) {
      auto *areg = PhysGPR(A0 + static_cast<uint8_t>(int_idx));
      Emit(RVOpcode::MV, areg, {val});
      call_ops.push_back(areg);
      int_idx++;
    } else {
      Emit(RVOpcode::SD, nullptr, {val, PhysGPR(SP), ctx_->Imm(static_cast<int>(stack_slot * 8))});
      stack_slot++;
    }
  }
  Emit(RVOpcode::CALL, nullptr, call_ops);
  if (inst->GetResult() != nullptr) {
    if (inst->GetResult()->IsFloat()) {
      Emit(RVOpcode::FMV_S, GetOrCreateVReg(inst->GetResult()), {PhysFPR(static_cast<uint8_t>(FPR::Fa0))});
    } else {
      Emit(RVOpcode::MV, GetOrCreateVReg(inst->GetResult()), {PhysGPR(A0)});
    }
  }
}

auto RISCVISel::GetOrCreateVReg(backend::utils::Operand *mir_operand) -> TargetOperand * {
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

auto RISCVISel::GetReg(backend::utils::Operand *mir_operand) -> TargetOperand * {
  if (mir_operand->IsRegister()) {
    return GetOrCreateVReg(mir_operand);
  }
  if (mir_operand->IsLiteral()) {
    // A float literal is materialized as its 32-bit pattern in a GPR, then moved
    // into an FPR (RISC-V cannot load a float immediate directly).
    if (auto *flit = dynamic_cast<backend::utils::FloatLiteral *>(mir_operand)) {
      float value = flit->GetValue();
      int bits = 0;
      std::memcpy(&bits, &value, sizeof(bits));
      auto *greg = ctx_->NewVirtualReg(TargetRegClass::General, TargetRegSize::DWord);
      Emit(RVOpcode::LI, greg, {ctx_->Imm(bits)});
      auto *freg = ctx_->NewVirtualReg(TargetRegClass::Float, TargetRegSize::DWord);
      Emit(RVOpcode::FMV_W_X, freg, {greg});
      return freg;
    }
    // MIR literal operands do not carry a type, so IsInt()/IsBool() cannot be
    // trusted here; dispatch on the concrete literal subtype instead.
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
    Emit(RVOpcode::LI, vreg, {ctx_->Imm(value)});
    return vreg;
  }
  // Frame slot / global / GEP result: the "value" is the address it denotes.
  if (mir_operand->IsFrame() || mir_operand->IsGlobal() || mir_operand->IsMemory()) {
    return LowerAddress(mir_operand);
  }
  Comment("TODO: unhandled operand kind");
  return Zero();
}

auto RISCVISel::Zero() -> TargetOperand * { return PhysGPR(ZERO); }

auto RISCVISel::PhysGPR(uint8_t id) -> TargetOperand * {
  return ctx_->PhysReg(TargetRegClass::General, TargetRegSize::QWord, id);
}

auto RISCVISel::PhysFPR(uint8_t id) -> TargetOperand * {
  return ctx_->PhysReg(TargetRegClass::Float, TargetRegSize::DWord, id);
}

auto RISCVISel::LabelForBlock(size_t block_id) -> TargetLabel * {
  return ctx_->Label(".L_" + func_name_ + "_" + std::to_string(block_id));
}

auto RISCVISel::Emit(RVOpcode opcode, TargetOperand *result, const std::vector<TargetOperand *> &operands)
    -> RISCVInst * {
  auto *inst = ctx_->CreateInst<RISCVInst>(opcode);
  if (result != nullptr) {
    inst->SetResult(result);
  }
  for (auto *op : operands) {
    inst->AddOperand(op);
  }
  cur_block_->AddInst(inst);
  return inst;
}

auto RISCVISel::Comment(const std::string &text) -> void {
  auto *inst = ctx_->CreateInst<RISCVInst>(RVOpcode::COMMENT);
  inst->SetComment(text);
  cur_block_->AddInst(inst);
}

auto RISCVISel::SelectLoad(backend::inst::Instruction *inst) -> void {
  auto *addr = LowerAddress(inst->GetOperands()[0]);
  auto *res = GetOrCreateVReg(inst->GetResult());
  auto opcode = inst->GetResult()->IsFloat() ? RVOpcode::FLW
                : inst->GetResult()->IsPtr()  ? RVOpcode::LD
                                              : RVOpcode::LW;
  Emit(opcode, res, {addr, ctx_->Imm(0)});
}

auto RISCVISel::SelectStore(backend::inst::Instruction *inst) -> void {
  auto *value = GetReg(inst->GetOperands()[0]);
  auto *addr = LowerAddress(inst->GetOperands()[1]);
  // Width follows the value's register (class/size), which is reliable where the
  // MIR operand type is not.
  auto opcode = IsFloatVReg(value) ? RVOpcode::FSW : IsWideVReg(value) ? RVOpcode::SD : RVOpcode::SW;
  Emit(opcode, nullptr, {value, addr, ctx_->Imm(0)});
}

auto RISCVISel::AllocaOffset(backend::utils::Operand *frame_op) -> size_t {
  auto it = alloca_offset_.find(frame_op);
  if (it != alloca_offset_.end()) {
    return it->second;
  }
  auto *frame_operand = static_cast<backend::utils::FrameOperand *>(frame_op);
  auto *info = compiler_context_->GetMIRContext()->GetLowerContext()->GetFrameInfoFromFrameOperand(frame_operand);
  auto size = info->GetSize();
  // Alloca area sits just above the outgoing stack-argument area.
  auto offset = outgoing_bytes_ + alloca_bytes_;
  alloca_offset_[frame_op] = offset;
  alloca_bytes_ += (size + 7U) & ~static_cast<size_t>(7U);  // 8-byte aligned
  return offset;
}

auto RISCVISel::LowerAddress(backend::utils::Operand *ptr) -> TargetOperand * {
  if (ptr->IsFrame()) {
    // Address of a stack slot: sp + alloca offset (immediate-legalized).
    auto offset = static_cast<int>(AllocaOffset(ptr));
    auto *addr = ctx_->NewVirtualReg(TargetRegClass::General, TargetRegSize::QWord);
    auto *sp = PhysGPR(static_cast<uint8_t>(2));  // x2 = sp
    if (offset >= -2048 && offset <= 2047) {
      Emit(RVOpcode::ADDI, addr, {sp, ctx_->Imm(offset)});
    } else {
      Emit(RVOpcode::LI, addr, {ctx_->Imm(offset)});
      Emit(RVOpcode::ADD, addr, {sp, addr});
    }
    return addr;
  }
  if (ptr->IsGlobal()) {
    auto *addr = ctx_->NewVirtualReg(TargetRegClass::General, TargetRegSize::QWord);
    Emit(RVOpcode::LA, addr, {ctx_->Symbol(static_cast<backend::utils::GlobalOperand *>(ptr)->GetName())});
    return addr;
  }
  if (ptr->IsRegister()) {
    // A pointer already materialized in a register (e.g. an array parameter).
    return GetOrCreateVReg(ptr);
  }
  if (ptr->IsMemory()) {
    // GEP result: base address plus the linearized element offset. Strides are
    // right-aligned to the innermost dimension, which handles local arrays (with
    // a leading 0 index), globals, and pointer parameters (leading dim -1)
    // uniformly. See docs/backend_codegen_plan.md.
    auto *mem = static_cast<backend::utils::MemoryOperand *>(ptr);
    const auto &info = mem->GetAddressInfo();
    if (info.base_ptr_ == nullptr) {
      Comment("TODO: memory operand without base pointer");
      return Zero();
    }
    auto *addr = LowerAddress(info.base_ptr_);
    const auto &dims = info.dimensions_;
    const auto &indices = info.indices_;
    constexpr int ELEM = 4;  // int element (float arrays are also 4 bytes)

    auto m = indices.size();
    auto n = dims.size();
    if (m == 0) {
      return addr;  // no indices: the address is just the base
    }
    std::vector<long> strides(m, ELEM);
    for (size_t k = m - 1; k > 0; --k) {
      long dim = 1;
      auto dim_idx = static_cast<long>(n) - static_cast<long>(m) + static_cast<long>(k);
      if (dim_idx >= 0 && dim_idx < static_cast<long>(n)) {
        dim = dims[dim_idx];
      }
      strides[k - 1] = strides[k] * dim;
    }

    for (size_t k = 0; k < m; ++k) {
      // Skip constant-zero indices (the common leading array-decay index).
      auto *lit = dynamic_cast<backend::utils::IntLiteral *>(indices[k]);
      if (lit != nullptr && lit->GetValue() == 0) {
        continue;
      }
      auto *idx_reg = GetReg(indices[k]);
      auto *term = ctx_->NewVirtualReg(TargetRegClass::General, TargetRegSize::QWord);
      auto *stride_reg = ctx_->NewVirtualReg(TargetRegClass::General, TargetRegSize::QWord);
      Emit(RVOpcode::LI, stride_reg, {ctx_->Imm(static_cast<int>(strides[k]))});
      Emit(RVOpcode::MUL, term, {idx_reg, stride_reg});
      auto *next = ctx_->NewVirtualReg(TargetRegClass::General, TargetRegSize::QWord);
      Emit(RVOpcode::ADD, next, {addr, term});
      addr = next;
    }
    return addr;
  }
  Comment("TODO: unhandled address kind");
  return Zero();
}

auto RISCVISel::IsFusableToBranch(backend::inst::Instruction *icmp) const -> bool {
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

}  // namespace backend::targetir::riscv
