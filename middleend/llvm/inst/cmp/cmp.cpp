#include "middleend/llvm/inst/cmp/cmp.h"
#include "compiler_context.h"
#include "context/llvmir_context.h"
namespace middleend::llvm::inst {

static inline auto GetPredicateStringForInt(CmpPredicate predicate) -> std::string {
  switch (predicate) {
    case CmpPredicate::EQ:
      return "seq";
    case CmpPredicate::NE:
      return "sne";
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
  result_->Emit(out);
  *out << " = icmp ";

  auto predicate_str = GetPredicateStringForInt(predicate_);
  *out << predicate_str << " ";

  auto type = operands_[0]->GetType();
  *out << utils::Operand::GetTypeInLLVMIR(type) << " ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
}

auto ICmpInst::Verify(context::CompilerContext *context) const -> bool {
  // Verify the result operand is a register operand
  if (result_ == nullptr || !result_->IsRegister()) {
    context->ReportError("ICmp instruction must have a valid result operand");
    return false;
  }
  // Verify the operands have exactly 2 operands, and both are int32
  if (operands_.size() != 2) {
    context->ReportError("ICmp instruction must have exactly 2 operands");
    return false;
  }

  if (GetParentBlock() == nullptr) {
    context->ReportError("ICmp instruction must have a parent block");
    return false;
  }

  return operands_[0]->IsInt32() && operands_[1]->IsInt32();
}

auto ICmpInst::Create(context::LLVMIRContext *context, CmpPredicate predicate, utils::Operand *operand1,
                      utils::Operand *operand2) -> ICmpInst * {
  auto *inst = new ICmpInst(OpKind::ICmp);
  inst->predicate_ = predicate;
  auto res = context->GetNewRegOperand(context->GetIntegerType());
  inst->SetResult(res);
  inst->AddOperand(operand1);
  inst->AddOperand(operand2);
  return inst;
}

// Float comparison
auto FCmpInst::Emit(std::ofstream *out) const -> void {
  result_->Emit(out);
  *out << " = fcmp ";

  auto predicate_str = GetPredicateStringForFloat(predicate_);
  *out << predicate_str << " ";

  auto type = operands_[0]->GetType();
  *out << utils::Operand::GetTypeInLLVMIR(type) << " ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
}

auto FCmpInst::Verify(context::CompilerContext *context) const -> bool {
  // Verify the result operand is a register operand
  if (result_ == nullptr || !result_->IsRegister()) {
    context->ReportError("FCmp instruction must have a valid result operand");
    return false;
  }
  // Verify the operands have exactly 2 operands, and both are float
  if (operands_.size() != 2) {
    context->ReportError("FCmp instruction must have exactly 2 operands");
    return false;
  }

  if (GetParentBlock() == nullptr) {
    context->ReportError("FCmp instruction must have a parent block");
    return false;
  }

  return operands_[0]->IsFloat() && operands_[1]->IsFloat();
}

auto FCmpInst::Create(context::LLVMIRContext *context, CmpPredicate predicate, utils::Operand *operand1,
                      utils::Operand *operand2) -> FCmpInst * {
  auto *inst = new FCmpInst(OpKind::FCmp);
  inst->predicate_ = predicate;
  auto res = context->GetNewRegOperand(context->GetFloatType());
  inst->SetResult(res);
  inst->AddOperand(operand1);
  inst->AddOperand(operand2);
  return inst;
}

}  // namespace middleend::llvm::inst