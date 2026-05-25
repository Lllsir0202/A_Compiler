#include "backend/mir/inst/cmp/cmp.h"
#include "context/compiler_context.h"
#include "mir_context.h"
namespace backend::inst {
auto ICmpInst::Verify(context::CompilerContext *context) const -> bool {
  // The icmp instruction should have exactly two operands, which are the lhs and rhs
  if (operands_.size() != 2) {
    context->Print("Operand size of icmp instruction should be 2, but got " + std::to_string(operands_.size()));
    return false;
  }
  // The predicate should be valid
  if (op_kind_ != OpKind::ICmp) {
    context->Print("Op kind of icmp instruction should be ICmp, but got " + std::to_string(static_cast<int>(op_kind_)));
    return false;
  }
  return true;
}

auto ICmpInst::Create(context::MIRContext *context, utils::Operand *lhs, utils::Operand *rhs,
                      CmpPredicate predicate) -> ICmpInst * {
  auto *inst = new ICmpInst(predicate);
  inst->AddOperand(lhs);
  inst->AddOperand(rhs);
  inst->op_kind_ = OpKind::ICmp;
  auto res = context->GetNewRegOperand(context->GetBoolType());
  inst->SetResult(res);
  res->SetDefInst(inst);
  return inst;
}

auto FCmpInst::Verify(context::CompilerContext *context) const -> bool {
  // The fcmp instruction should have exactly two operands, which are the lhs and rhs
  if (operands_.size() != 2) {
    context->Print("Operand size of fcmp instruction should be 2, but got " + std::to_string(operands_.size()));
    return false;
  }
  // The predicate should be valid
  if (op_kind_ != OpKind::FCmp) {
    context->Print("Op kind of fcmp instruction should be FCmp, but got " + std::to_string(static_cast<int>(op_kind_)));
    return false;
  }
  return true;
}

auto FCmpInst::Create(context::MIRContext *context, utils::Operand *lhs, utils::Operand *rhs,
                      CmpPredicate predicate) -> FCmpInst * {
  auto *inst = new FCmpInst(predicate);
  inst->AddOperand(lhs);
  inst->AddOperand(rhs);
  inst->op_kind_ = OpKind::FCmp;
  auto res = context->GetNewRegOperand(context->GetBoolType());
  inst->SetResult(res);
  res->SetDefInst(inst);
  return inst;
}

auto CmpPredicateFromLLVM(middleend::llvm::inst::CmpPredicate llvm_pred) -> CmpPredicate {
  switch (llvm_pred) {
    case middleend::llvm::inst::CmpPredicate::EQ:
      return CmpPredicate::EQ;
    case middleend::llvm::inst::CmpPredicate::NE:
      return CmpPredicate::NE;
    case middleend::llvm::inst::CmpPredicate::LT:
      return CmpPredicate::LT;
    case middleend::llvm::inst::CmpPredicate::LE:
      return CmpPredicate::LE;
    case middleend::llvm::inst::CmpPredicate::GT:
      return CmpPredicate::GT;
    case middleend::llvm::inst::CmpPredicate::GE:
      return CmpPredicate::GE;
    default:
      throw std::invalid_argument("Invalid LLVM comparison predicate");
  }
}

}  // namespace backend::inst