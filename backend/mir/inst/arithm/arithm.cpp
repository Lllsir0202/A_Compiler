#include "backend/mir/inst/arithm/arithm.h"
#include "backend/mir/utils/operand/operand.h"
#include "compiler_context.h"
#include "mir_context.h"

namespace backend::inst {

auto AddInst::Verify(context::CompilerContext *context) const -> bool {
  // Verify the result operand is a register operand
  if (result_ == nullptr || !result_->IsRegister()) {
    context->Print("Add instruction must have a valid result operand\n");
    return false;
  }
  // Verify the operands have exactly 2 operands, and both are int32
  if (operands_.size() != 2) {
    context->Print("Add instruction must have exactly 2 operands\n");
    return false;
  }
  // verify the operands are not nullptr
  if (operands_[0] == nullptr || operands_[1] == nullptr) {
    context->Print("Add instruction operands cannot be null\n");
    return false;
  }
  if (GetParentBlock() == nullptr) {
    context->Print("Add instruction must have a parent block\n");
    return false;
  }
  // Check the instruction
  if (result_->GetDefInst() != this) {
    context->Print("Add instruction's result operand must point to itself\n");
    return false;
  }

  return operands_[0]->IsInt() && operands_[1]->IsInt();
}

auto AddInst::Create(context::MIRContext *context, utils::Operand *operand1, utils::Operand *operand2) -> AddInst * {
    auto inst = new AddInst();
    inst->operands_.reserve(2);
    inst->AddOperand(operand1);
    inst->AddOperand(operand2);
    auto res = context

}



}  // namespace backend::inst