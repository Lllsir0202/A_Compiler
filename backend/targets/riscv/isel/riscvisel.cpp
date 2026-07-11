#include "backend/targets/riscv/isel/riscvisel.h"
#include <string>
#include "backend/mir/inst/arithm/arithm.h"
#include "backend/mir/inst/branch/branch.h"
#include "backend/mir/inst/branch/ret.h"
#include "backend/mir/inst/cmp/cmp.h"
#include "backend/mir/inst/func/call.h"
#include "backend/mir/inst/func/funcdef.h"
#include "backend/mir/inst/inst.h"
#include "backend/mir/inst/pseudo/copy.h"
#include "backend/mir/utils/operand/labeloperand.h"
#include "backend/mir/utils/operand/literaloperand.h"
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
constexpr size_t MAX_REG_ARGS = 8;
}  // namespace

auto RISCVISel::SelectFunction(backend::utils::Function *mir_func) -> TargetFunction * {
  func_name_ = mir_func->GetName();
  value_map_.clear();
  block_by_id_.clear();
  fused_icmps_.clear();
  use_count_.clear();
  one_user_.clear();

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
      size_t idx = 0;
      for (auto *param : func_def->GetOperands()) {
        if (param->IsFloat()) {
          Comment("TODO: float parameter passing");
          continue;
        }
        if (idx >= MAX_REG_ARGS) {
          Comment("TODO: stack-passed parameter " + std::to_string(idx));
          idx++;
          continue;
        }
        Emit(RVOpcode::MV, GetOrCreateVReg(param), {PhysGPR(A0 + static_cast<uint8_t>(idx))});
        idx++;
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
    case OpKind::ICmp: SelectICmp(inst); break;
    case OpKind::CondBr: SelectCondBr(inst); break;
    case OpKind::Br: SelectBr(inst); break;
    case OpKind::Ret: SelectRet(inst); break;
    case OpKind::Copy: SelectCopy(inst); break;
    case OpKind::Zext: SelectCopy(inst); break;  // i1 -> i32 is a value-preserving move
    case OpKind::Call: SelectCall(inst); break;
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
      Comment("TODO: float return value");
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
  Emit(RVOpcode::MV, GetOrCreateVReg(inst->GetResult()), {src});
}

auto RISCVISel::SelectCall(backend::inst::Instruction *inst) -> void {
  auto *call = static_cast<backend::inst::CallInst *>(inst);
  size_t idx = 0;
  for (auto *param : inst->GetOperands()) {
    if (param->IsFloat()) {
      Comment("TODO: float argument passing");
      continue;
    }
    if (idx >= MAX_REG_ARGS) {
      Comment("TODO: stack-passed argument " + std::to_string(idx));
      idx++;
      continue;
    }
    Emit(RVOpcode::MV, PhysGPR(A0 + static_cast<uint8_t>(idx)), {GetReg(param)});
    idx++;
  }
  Emit(RVOpcode::CALL, nullptr, {ctx_->Symbol(call->GetCallee())});
  if (inst->GetResult() != nullptr && !inst->GetResult()->IsFloat()) {
    Emit(RVOpcode::MV, GetOrCreateVReg(inst->GetResult()), {PhysGPR(A0)});
  } else if (inst->GetResult() != nullptr) {
    Comment("TODO: float return value from call");
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
  Comment("TODO: unhandled operand kind");
  return Zero();
}

auto RISCVISel::Zero() -> TargetOperand * { return PhysGPR(ZERO); }

auto RISCVISel::PhysGPR(uint8_t id) -> TargetOperand * {
  return ctx_->PhysReg(TargetRegClass::General, TargetRegSize::QWord, id);
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
