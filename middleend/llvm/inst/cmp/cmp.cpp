#include "middleend/llvm/inst/cmp/cmp.h"
#include "compiler_context.h"
#include "context/llvmir_context.h"
namespace middleend::llvm::inst {

static inline auto GetPredicateStringForInt(CmpPredicate predicate) -> std::string {
  switch (predicate) {
    case CmpPredicate::EQ:
      return "eq";
    case CmpPredicate::NE:
      return "ne";
    case CmpPredicate::LT:
      return "slt";
    case CmpPredicate::LE:
      return "sle";
    case CmpPredicate::GT:
      return "sgt";
    case CmpPredicate::GE:
      return "sge";
    default:
      throw std::invalid_argument("Invalid predicate");
      return "";
  }
}

static inline auto GetPredicateStringForFloat(CmpPredicate predicate) -> std::string {
  switch (predicate) {
    case CmpPredicate::EQ:
      return "oeq";
    case CmpPredicate::NE:
      return "one";
    case CmpPredicate::LT:
      return "olt";
    case CmpPredicate::LE:
      return "ole";
    case CmpPredicate::GT:
      return "ogt";
    case CmpPredicate::GE:
      return "oge";
    default:
      throw std::invalid_argument("Invalid predicate");
      return "";
  }
}

// Int comparison
auto ICmpInst::Emit(std::ofstream *out) const -> void {
  *out << "  ";
  result_->Emit(out);
  *out << " = icmp ";

  auto predicate_str = GetPredicateStringForInt(predicate_);
  *out << predicate_str << " ";

  auto type = operands_[0]->GetType();
  *out << utils::Operand::GetTypeInLLVMIR(type) << " ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << "\n";
}

auto ICmpInst::Verify(context::CompilerContext *context) const -> bool {
  // Verify the result operand is a register operand
  if (result_ == nullptr || !result_->IsRegister()) {
    context->Print("ICmp instruction must have a valid result operand");
    return false;
  }
  // Verify the operands have exactly 2 operands, and both are int32
  if (operands_.size() != 2) {
    context->Print("ICmp instruction must have exactly 2 operands");
    return false;
  }

  if (!operands_[0]->IsInt32() || !operands_[1]->IsInt32()) {
    context->Print("ICmp instruction operands must be int32");
    return false;
  }

  if (GetParentBlock() == nullptr) {
    context->Print("ICmp instruction must have a parent block");
    return false;
  }

  if (result_->GetInstruction() != this) {
    context->Print("ICmp instruction result operand must point back to the instruction");
    return false;
  }

  return operands_[0]->IsInt32() && operands_[1]->IsInt32();
}

auto ICmpInst::Create(context::LLVMIRContext *context, CmpPredicate predicate, utils::Operand *operand1,
                      utils::Operand *operand2) -> ICmpInst * {
  auto *inst = new ICmpInst(OpKind::ICmp);
  inst->predicate_ = predicate;
  // ICmp returns a boolean value
  auto res = context->GetNewRegOperand(context->GetBoolType());
  inst->SetResult(res);
  res->SetInstruction(inst);
  inst->AddOperand(operand1);
  inst->AddOperand(operand2);
  return inst;
}

// Float comparison
auto FCmpInst::Emit(std::ofstream *out) const -> void {
  *out << "  ";
  result_->Emit(out);
  *out << " = fcmp ";

  auto predicate_str = GetPredicateStringForFloat(predicate_);
  *out << predicate_str << " ";

  auto type = operands_[0]->GetType();
  *out << utils::Operand::GetTypeInLLVMIR(type) << " ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << "\n";
}

auto FCmpInst::Verify(context::CompilerContext *context) const -> bool {
  // Verify the result operand is a register operand
  if (result_ == nullptr || !result_->IsRegister()) {
    context->Print("FCmp instruction must have a valid result operand");
    return false;
  }
  // Verify the operands have exactly 2 operands, and both are float
  if (operands_.size() != 2) {
    context->Print("FCmp instruction must have exactly 2 operands");
    return false;
  }

  if (!operands_[0]->IsFloat() || !operands_[1]->IsFloat()) {
    context->Print("FCmp instruction operands must be float");
    return false;
  }

  if (GetParentBlock() == nullptr) {
    context->Print("FCmp instruction must have a parent block");
    return false;
  }

  if (result_->GetInstruction() != this) {
    context->Print("FCmp instruction result operand must point back to the instruction");
    return false;
  }

  return operands_[0]->IsFloat() && operands_[1]->IsFloat();
}

auto FCmpInst::Create(context::LLVMIRContext *context, CmpPredicate predicate, utils::Operand *operand1,
                      utils::Operand *operand2) -> FCmpInst * {
  auto *inst = new FCmpInst(OpKind::FCmp);
  inst->predicate_ = predicate;
  auto res = context->GetNewRegOperand(context->GetBoolType());
  inst->SetResult(res);
  res->SetInstruction(inst);
  inst->AddOperand(operand1);
  inst->AddOperand(operand2);
  return inst;
}

auto XorInst::Emit(std::ofstream *out) const -> void {
  *out << "  ";
  result_->Emit(out);
  *out << " = xor ";
  auto type = operands_[0]->GetType();
  *out << utils::Operand::GetTypeInLLVMIR(type) << " ";
  operands_[0]->Emit(out);
  *out << ", ";
  if (operands_.size() > 1) {
    operands_[1]->Emit(out);
  } else {
    *out << "true";
  }
  *out << "\n";
}

auto XorInst::Verify(context::CompilerContext *context) const -> bool {
  // Verify the result operand is a register operand
  if (result_ == nullptr || !result_->IsRegister()) {
    context->Print("Xor instruction must have a valid result operand");
    return false;
  }
  // Verify the operands have exactly 2 operands, and both are int32 or bool
  if (operands_.size() != 2) {
    context->Print("Xor instruction must have exactly 2 operands");
    return false;
  }

  if (!operands_[0]->IsInt32() && !operands_[0]->IsBool()) {
    context->Print("Xor instruction first operand must be int32 or bool");
    return false;
  }

  if (!operands_[1]->IsInt32() && !operands_[1]->IsBool()) {
    context->Print("Xor instruction second operand must be int32 or bool");
    return false;
  }

  if (GetParentBlock() == nullptr) {
    context->Print("Xor instruction must have a parent block");
    return false;
  }

  if (result_->GetInstruction() != this) {
    context->Print("Xor instruction result operand must point back to the instruction");
    return false;
  }

  return true;
}

auto XorInst::Create(context::LLVMIRContext *context, utils::Operand *operand1, utils::Operand *operand2) -> XorInst * {
  auto *inst = new XorInst(OpKind::Xor);
  auto res = context->GetNewRegOperand(operand1->GetType());
  inst->SetResult(res);
  res->SetInstruction(inst);
  inst->AddOperand(operand1);
  inst->AddOperand(operand2);
  return inst;
}

// Used in lowering not operation
auto XorInst::Create(context::LLVMIRContext *context, utils::Operand *operand) -> XorInst * {
  auto *inst = new XorInst(OpKind::Xor);
  auto res = context->GetNewRegOperand(operand->GetType());
  inst->SetResult(res);
  res->SetInstruction(inst);
  inst->AddOperand(operand);
  return inst;
}

}  // namespace middleend::llvm::inst