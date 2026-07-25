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
  auto res = context->GetNewRegOperand(context->GetIntegerType());
  inst->SetResult(res);
  res->SetDefInst(inst);
  return inst;
}

auto AddInst::Emit(std::ofstream *out) const -> void {
  *out << "  ";
  result_->Emit(out);
  *out << " = add ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << "\n";
}

auto SubInst::Verify(context::CompilerContext *context) const -> bool {
  // Verify the result operand is a register operand
  if (result_ == nullptr || !result_->IsRegister()) {
    context->Print("Sub instruction must have a valid result operand\n");
    return false;
  }
  // Verify the operands have exactly 2 operands, and both are int32
  if (operands_.size() != 2) {
    context->Print("Sub instruction must have exactly 2 operands\n");
    return false;
  }
  // verify the operands are not nullptr
  if (operands_[0] == nullptr || operands_[1] == nullptr) {
    context->Print("Sub instruction operands cannot be null\n");
    return false;
  }
  if (GetParentBlock() == nullptr) {
    context->Print("Sub instruction must have a parent block\n");
    return false;
  }
  // Check the instruction
  if (result_->GetDefInst() != this) {
    context->Print("Sub instruction's result operand must point to itself\n");
    return false;
  }

  return operands_[0]->IsInt() && operands_[1]->IsInt();
}

auto SubInst::Create(context::MIRContext *context, utils::Operand *operand1, utils::Operand *operand2) -> SubInst * {
  auto inst = new SubInst();
  inst->operands_.reserve(2);
  inst->AddOperand(operand1);
  inst->AddOperand(operand2);
  auto res = context->GetNewRegOperand(context->GetIntegerType());
  inst->SetResult(res);
  res->SetDefInst(inst);
  return inst;
}

auto SubInst::Emit(std::ofstream *out) const -> void {
  *out << "  ";
  result_->Emit(out);
  *out << " = sub ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << "\n";
}

auto MulInst::Verify(context::CompilerContext *context) const -> bool {
  // Verify the result operand is a register operand
  if (result_ == nullptr || !result_->IsRegister()) {
    context->Print("Mul instruction must have a valid result operand\n");
    return false;
  }
  // Verify the operands have exactly 2 operands, and both are int32
  if (operands_.size() != 2) {
    context->Print("Mul instruction must have exactly 2 operands\n");
    return false;
  }
  // verify the operands are not nullptr
  if (operands_[0] == nullptr || operands_[1] == nullptr) {
    context->Print("Mul instruction operands cannot be null\n");
    return false;
  }
  if (GetParentBlock() == nullptr) {
    context->Print("Mul instruction must have a parent block\n");
    return false;
  }
  // Check the instruction
  if (result_->GetDefInst() != this) {
    context->Print("Mul instruction's result operand must point to itself\n");
    return false;
  }

  return operands_[0]->IsInt() && operands_[1]->IsInt();
}

auto MulInst::Create(context::MIRContext *context, utils::Operand *operand1, utils::Operand *operand2) -> MulInst * {
  auto inst = new MulInst();
  inst->operands_.reserve(2);
  inst->AddOperand(operand1);
  inst->AddOperand(operand2);
  auto res = context->GetNewRegOperand(context->GetIntegerType());
  inst->SetResult(res);
  res->SetDefInst(inst);
  return inst;
}

auto MulInst::Emit(std::ofstream *out) const -> void {
  *out << "  ";
  result_->Emit(out);
  *out << " = mul ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << "\n";
}

auto DivInst::Verify(context::CompilerContext *context) const -> bool {
  // Verify the result operand is a register operand
  if (result_ == nullptr || !result_->IsRegister()) {
    context->Print("Div instruction must have a valid result operand\n");
    return false;
  }
  // Verify the operands have exactly 2 operands, and both are int32
  if (operands_.size() != 2) {
    context->Print("Div instruction must have exactly 2 operands\n");
    return false;
  }
  // verify the operands are not nullptr
  if (operands_[0] == nullptr || operands_[1] == nullptr) {
    context->Print("Div instruction operands cannot be null\n");
    return false;
  }
  if (GetParentBlock() == nullptr) {
    context->Print("Div instruction must have a parent block\n");
    return false;
  }
  // Check the instruction
  if (result_->GetDefInst() != this) {
    context->Print("Div instruction's result operand must point to itself\n");
    return false;
  }

  return operands_[0]->IsInt() && operands_[1]->IsInt();
}

auto DivInst::Create(context::MIRContext *context, utils::Operand *operand1, utils::Operand *operand2) -> DivInst * {
  auto inst = new DivInst();
  inst->operands_.reserve(2);
  inst->AddOperand(operand1);
  inst->AddOperand(operand2);
  auto res = context->GetNewRegOperand(context->GetIntegerType());
  inst->SetResult(res);
  res->SetDefInst(inst);
  return inst;
}

auto DivInst::Emit(std::ofstream *out) const -> void {
  *out << "  ";
  result_->Emit(out);
  *out << " = div ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << "\n";
}

auto ModInst::Verify(context::CompilerContext *context) const -> bool {
  // Verify the result operand is a register operand
  if (result_ == nullptr || !result_->IsRegister()) {
    context->Print("Mod instruction must have a valid result operand\n");
    return false;
  }
  // Verify the operands have exactly 2 operands, and both are int32
  if (operands_.size() != 2) {
    context->Print("Mod instruction must have exactly 2 operands\n");
    return false;
  }
  // verify the operands are not nullptr
  if (operands_[0] == nullptr || operands_[1] == nullptr) {
    context->Print("Mod instruction operands cannot be null\n");
    return false;
  }
  if (GetParentBlock() == nullptr) {
    context->Print("Mod instruction must have a parent block\n");
    return false;
  }
  // Check the instruction
  if (result_->GetDefInst() != this) {
    context->Print("Mod instruction's result operand must point to itself\n");
    return false;
  }

  return operands_[0]->IsInt() && operands_[1]->IsInt();
}

auto ModInst::Create(context::MIRContext *context, utils::Operand *operand1, utils::Operand *operand2) -> ModInst * {
  auto inst = new ModInst();
  inst->operands_.reserve(2);
  inst->AddOperand(operand1);
  inst->AddOperand(operand2);
  auto res = context->GetNewRegOperand(context->GetIntegerType());
  inst->SetResult(res);
  res->SetDefInst(inst);
  return inst;
}

auto ModInst::Emit(std::ofstream *out) const -> void {
  *out << "  ";
  result_->Emit(out);
  *out << " = mod ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << "\n";
}

auto AddFInst::Verify(context::CompilerContext *context) const -> bool {
  // Verify the result operand is a register operand
  if (result_ == nullptr || !result_->IsRegister()) {
    context->Print("AddF instruction must have a valid result operand\n");
    return false;
  }
  // Verify the operands have exactly 2 operands, and both are float
  if (operands_.size() != 2) {
    context->Print("AddF instruction must have exactly 2 operands\n");
    return false;
  }
  // verify the operands are not nullptr
  if (operands_[0] == nullptr || operands_[1] == nullptr) {
    context->Print("AddF instruction operands cannot be null\n");
    return false;
  }
  if (GetParentBlock() == nullptr) {
    context->Print("AddF instruction must have a parent block\n");
    return false;
  }
  // Check the instruction
  if (result_->GetDefInst() != this) {
    context->Print("AddF instruction's result operand must point to itself\n");
    return false;
  }

  return operands_[0]->IsFloat() && operands_[1]->IsFloat();
}

auto AddFInst::Create(context::MIRContext *context, utils::Operand *operand1, utils::Operand *operand2) -> AddFInst * {
  auto inst = new AddFInst();
  inst->operands_.reserve(2);
  inst->AddOperand(operand1);
  inst->AddOperand(operand2);
  auto res = context->GetNewRegOperand(context->GetFloatType());
  inst->SetResult(res);
  res->SetDefInst(inst);
  return inst;
}

auto AddFInst::Emit(std::ofstream *out) const -> void {
  *out << "  ";
  result_->Emit(out);
  *out << " = fadd ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << "\n";
}

auto SubFInst::Verify(context::CompilerContext *context) const -> bool {
  // Verify the result operand is a register operand
  if (result_ == nullptr || !result_->IsRegister()) {
    context->Print("SubF instruction must have a valid result operand\n");
    return false;
  }
  // Verify the operands have exactly 2 operands, and both are float
  if (operands_.size() != 2) {
    context->Print("SubF instruction must have exactly 2 operands\n");
    return false;
  }
  // verify the operands are not nullptr
  if (operands_[0] == nullptr || operands_[1] == nullptr) {
    context->Print("SubF instruction operands cannot be null\n");
    return false;
  }
  if (GetParentBlock() == nullptr) {
    context->Print("SubF instruction must have a parent block\n");
    return false;
  }
  // Check the instruction
  if (result_->GetDefInst() != this) {
    context->Print("SubF instruction's result operand must point to itself\n");
    return false;
  }

  return operands_[0]->IsFloat() && operands_[1]->IsFloat();
}

auto SubFInst::Create(context::MIRContext *context, utils::Operand *operand1, utils::Operand *operand2) -> SubFInst * {
  auto inst = new SubFInst();
  inst->operands_.reserve(2);
  inst->AddOperand(operand1);
  inst->AddOperand(operand2);
  auto res = context->GetNewRegOperand(context->GetFloatType());
  inst->SetResult(res);
  res->SetDefInst(inst);
  return inst;
}

auto SubFInst::Emit(std::ofstream *out) const -> void {
  *out << "  ";
  result_->Emit(out);
  *out << " = fsub ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << "\n";
}

auto MulFInst::Verify(context::CompilerContext *context) const -> bool {
  // Verify the result operand is a register operand
  if (result_ == nullptr || !result_->IsRegister()) {
    context->Print("MulF instruction must have a valid result operand\n");
    return false;
  }
  // Verify the operands have exactly 2 operands, and both are float
  if (operands_.size() != 2) {
    context->Print("MulF instruction must have exactly 2 operands\n");
    return false;
  }
  // verify the operands are not nullptr
  if (operands_[0] == nullptr || operands_[1] == nullptr) {
    context->Print("MulF instruction operands cannot be null\n");
    return false;
  }
  if (GetParentBlock() == nullptr) {
    context->Print("MulF instruction must have a parent block\n");
    return false;
  }
  // Check the instruction
  if (result_->GetDefInst() != this) {
    context->Print("MulF instruction's result operand must point to itself\n");
    return false;
  }

  return operands_[0]->IsFloat() && operands_[1]->IsFloat();
}

auto MulFInst::Create(context::MIRContext *context, utils::Operand *operand1, utils::Operand *operand2) -> MulFInst * {
  auto inst = new MulFInst();
  inst->operands_.reserve(2);
  inst->AddOperand(operand1);
  inst->AddOperand(operand2);
  auto res = context->GetNewRegOperand(context->GetFloatType());
  inst->SetResult(res);
  res->SetDefInst(inst);
  return inst;
}

auto MulFInst::Emit(std::ofstream *out) const -> void {
  *out << "  ";
  result_->Emit(out);
  *out << " = fmul ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << "\n";
}

auto DivFInst::Verify(context::CompilerContext *context) const -> bool {
  // Verify the result operand is a register operand
  if (result_ == nullptr || !result_->IsRegister()) {
    context->Print("DivF instruction must have a valid result operand\n");
    return false;
  }
  // Verify the operands have exactly 2 operands, and both are float
  if (operands_.size() != 2) {
    context->Print("DivF instruction must have exactly 2 operands\n");
    return false;
  }
  // verify the operands are not nullptr
  if (operands_[0] == nullptr || operands_[1] == nullptr) {
    context->Print("DivF instruction operands cannot be null\n");
    return false;
  }
  if (GetParentBlock() == nullptr) {
    context->Print("DivF instruction must have a parent block\n");
    return false;
  }
  // Check the instruction
  if (result_->GetDefInst() != this) {
    context->Print("DivF instruction's result operand must point to itself\n");
    return false;
  }

  return operands_[0]->IsFloat() && operands_[1]->IsFloat();
}

auto DivFInst::Create(context::MIRContext *context, utils::Operand *operand1, utils::Operand *operand2) -> DivFInst * {
  auto inst = new DivFInst();
  inst->operands_.reserve(2);
  inst->AddOperand(operand1);
  inst->AddOperand(operand2);
  auto res = context->GetNewRegOperand(context->GetFloatType());
  inst->SetResult(res);
  res->SetDefInst(inst);
  return inst;
}

auto DivFInst::Emit(std::ofstream *out) const -> void {
  *out << "  ";
  result_->Emit(out);
  *out << " = fdiv ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << "\n";
}

#define DEFINE_SHIFT_MIR_INST(NAME, MNEMONIC)                                                               \
  auto NAME##Inst::Verify(context::CompilerContext *context) const->bool {                                  \
    if (result_ == nullptr || !result_->IsRegister() || operands_.size() != 2 || operands_[0] == nullptr || \
        operands_[1] == nullptr) {                                                                          \
      context->Print(#NAME " instruction has invalid operands or result\n");                                \
      return false;                                                                                         \
    }                                                                                                       \
    if (GetParentBlock() == nullptr || result_->GetDefInst() != this) {                                     \
      context->Print(#NAME " instruction is not attached consistently\n");                                  \
      return false;                                                                                         \
    }                                                                                                       \
    return operands_[0]->IsInt() && operands_[1]->IsInt();                                                  \
  }                                                                                                         \
                                                                                                            \
  auto NAME##Inst::Create(context::MIRContext *context, utils::Operand *operand1, utils::Operand *operand2) \
      ->NAME##Inst * {                                                                                      \
    auto *instruction = new NAME##Inst();                                                                   \
    instruction->operands_.reserve(2);                                                                      \
    instruction->AddOperand(operand1);                                                                      \
    instruction->AddOperand(operand2);                                                                      \
    auto *result = context->GetNewRegOperand(context->GetIntegerType());                                    \
    instruction->SetResult(result);                                                                         \
    result->SetDefInst(instruction);                                                                        \
    return instruction;                                                                                     \
  }                                                                                                         \
                                                                                                            \
  auto NAME##Inst::Emit(std::ofstream *out) const->void {                                                   \
    *out << "  ";                                                                                           \
    result_->Emit(out);                                                                                     \
    *out << " = " MNEMONIC " ";                                                                             \
    operands_[0]->Emit(out);                                                                                \
    *out << ", ";                                                                                           \
    operands_[1]->Emit(out);                                                                                \
    *out << "\n";                                                                                           \
  }

DEFINE_SHIFT_MIR_INST(Shl, "shl")
DEFINE_SHIFT_MIR_INST(LShr, "lshr")
DEFINE_SHIFT_MIR_INST(AShr, "ashr")

#undef DEFINE_SHIFT_MIR_INST

}  // namespace backend::inst
