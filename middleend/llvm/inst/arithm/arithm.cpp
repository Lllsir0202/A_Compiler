#include "middleend/llvm/inst/arithm/arithm.h"
#include <fstream>
#include "compiler_context.h"
#include "llvmir_context.h"
#include "middleend/llvm/utils/operand/operand.h"

namespace middleend::llvm::inst {
auto AddInst::Emit(std::ofstream *out) const -> void {
  /**
   * @brief Emit the add instruction as a string
   * @note The format of the add instruction is:
   * %result = add i32 %op1, %op2
   * where %result is the result operand, i32 is the type of the operands,
   */

  *out << "  ";
  result_->Emit(out);
  *out << " = add ";

  /** @brief We will do verify before emitting, so we can make sure that currently
   *the instruction guarantees that:
   * 1. The result operand is a register operand
   * 2. The operands have exactly 2 operands, and both are int32
   * */
  auto type = operands_[0]->GetType();
  *out << utils::Operand::GetTypeInLLVMIR(type) << " ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << "\n";
}

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
  if (result_->GetInstruction() != this) {
    context->Print("Add instruction's result operand must point to itself\n");
    return false;
  }

  return operands_[0]->IsInt32() && operands_[1]->IsInt32();
}

auto AddInst::Create(context::LLVMIRContext *context, utils::Operand *operand1, utils::Operand *operand2) -> AddInst * {
  auto *inst = new AddInst();
  auto res = context->GetNewRegOperand(context->GetIntegerType());
  inst->SetResult(res);
  res->SetInstruction(inst);
  inst->AddOperand(operand1);
  inst->AddOperand(operand2);
  return inst;
}

// Float add
auto AddFInst::Emit(std::ofstream *out) const -> void {
  *out << "  ";
  result_->Emit(out);
  *out << " = fadd ";

  auto type = operands_[0]->GetType();
  *out << utils::Operand::GetTypeInLLVMIR(type) << " ";
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
  if (operands_[0] == nullptr || operands_[1] == nullptr) {
    context->Print("AddF instruction operands cannot be null\n");
    return false;
  }
  if (GetParentBlock() == nullptr) {
    context->Print("AddF instruction must have a parent block\n");
    return false;
  }

  if (result_->GetInstruction() != this) {
    context->Print("AddF instruction's result operand must point to itself\n");
    return false;
  }

  return operands_[0]->IsFloat() && operands_[1]->IsFloat();
}

auto AddFInst::Create(context::LLVMIRContext *context, utils::Operand *operand1, utils::Operand *operand2)
    -> AddFInst * {
  auto *inst = new AddFInst();
  auto res = context->GetNewRegOperand(context->GetFloatType());
  inst->SetResult(res);
  res->SetInstruction(inst);
  inst->AddOperand(operand1);
  inst->AddOperand(operand2);
  return inst;
}

auto SubInst::Emit(std::ofstream *out) const -> void {
  /**
   * @brief Emit the sub instruction
   */
  *out << "  ";
  result_->Emit(out);
  *out << " = sub ";
  // Has checked the operand size and type
  auto type = operands_[0]->GetType();
  *out << utils::Operand::GetTypeInLLVMIR(type) << " ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
  *out << "\n";
}

auto SubInst::Verify(context::CompilerContext *context) const -> bool {
  /**
   * @brief Verify the sub instruction
   * 1. The operand size should be 2
   * 2. The operand type should be the same and either int or float
   * 3. The result type should be the same as the operand type
   */

  // Verify the result operand is a register operand
  if (result_ == nullptr || !result_->IsRegister()) {
    context->Print("Sub instruction must have a valid result operand\n");
    return false;
  }

  if (!result_->IsInt32()) {
    context->Print("Sub instruction must have an int32 result operand\n ");
    return false;
  }

  // Verify the operands have exactly 2 operands, and both are int32
  if (operands_.size() != 2) {
    context->Print("Sub instruction must have exactly 2 operands\n");
    return false;
  }
  if (operands_[0] == nullptr || operands_[1] == nullptr) {
    context->Print("Sub instruction operands cannot be null\n");
    return false;
  }
  // Verify the parent block is not nullptr
  if (GetParentBlock() == nullptr) {
    context->Print("Sub instruction must have a parent block\n");
    return false;
  }

  if (result_->GetInstruction() != this) {
    context->Print("Sub instruction's result operand must point to itself\n ");
    return false;
  }

  return operands_[0]->IsInt32() && operands_[1]->IsInt32();
}

auto SubInst::Create(context::LLVMIRContext *context, utils::Operand *operand1, utils::Operand *operand2) -> SubInst * {
  auto *inst = new SubInst();
  auto res = context->GetNewRegOperand(context->GetIntegerType());
  inst->SetResult(res);
  res->SetInstruction(inst);
  inst->AddOperand(operand1);
  inst->AddOperand(operand2);
  return inst;
}

// Float sub
auto SubFInst::Emit(std::ofstream *out) const -> void {
  *out << "  ";
  result_->Emit(out);
  *out << " = fsub ";

  auto type = operands_[0]->GetType();
  *out << utils::Operand::GetTypeInLLVMIR(type) << " ";
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

  if (!result_->IsFloat()) {
    context->Print("SubF instruction must have a float result operand\n");
    return false;
  }

  // Verify the operands have exactly 2 operands, and both are float
  if (operands_.size() != 2) {
    context->Print("SubF instruction must have exactly 2 operands\n");
    return false;
  }
  if (operands_[0] == nullptr || operands_[1] == nullptr) {
    context->Print("SubF instruction operands cannot be null\n");
    return false;
  }
  if (GetParentBlock() == nullptr) {
    context->Print("SubF instruction must have a parent block\n");
    return false;
  }

  if (result_->GetInstruction() != this) {
    context->Print("SubF instruction's result operand must point to itself\n");
    return false;
  }

  return operands_[0]->IsFloat() && operands_[1]->IsFloat();
}

auto SubFInst::Create(context::LLVMIRContext *context, utils::Operand *operand1, utils::Operand *operand2)
    -> SubFInst * {
  auto *inst = new SubFInst();
  auto res = context->GetNewRegOperand(context->GetFloatType());
  inst->SetResult(res);
  res->SetInstruction(inst);
  inst->AddOperand(operand1);
  inst->AddOperand(operand2);
  return inst;
}

// Mul
auto MulInst::Emit(std::ofstream *out) const -> void {
  *out << "  ";
  result_->Emit(out);
  *out << " = mul ";

  auto type = operands_[0]->GetType();
  *out << utils::Operand::GetTypeInLLVMIR(type) << " ";
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

  if (!result_->IsInt32()) {
    context->Print("Mul instruction must have an int32 result operand\n");
    return false;
  }

  // Verify the operands have exactly 2 operands, and both are int32
  if (operands_.size() != 2) {
    context->Print("Mul instruction must have exactly 2 operands\n");
    return false;
  }
  if (operands_[0] == nullptr || operands_[1] == nullptr) {
    context->Print("Mul instruction operands cannot be null\n");
    return false;
  }
  if (GetParentBlock() == nullptr) {
    context->Print("Mul instruction must have a parent block\n");
    return false;
  }

  if (result_->GetInstruction() != this) {
    context->Print("Mul instruction's result operand must point to itself\n");
    return false;
  }

  return operands_[0]->IsInt32() && operands_[1]->IsInt32();
}

auto MulInst::Create(context::LLVMIRContext *context, utils::Operand *operand1, utils::Operand *operand2) -> MulInst * {
  auto *inst = new MulInst();
  auto res = context->GetNewRegOperand(context->GetIntegerType());
  inst->SetResult(res);
  res->SetInstruction(inst);
  inst->AddOperand(operand1);
  inst->AddOperand(operand2);
  return inst;
}

// Float mul
auto MulFInst::Emit(std::ofstream *out) const -> void {
  *out << "  ";
  result_->Emit(out);
  *out << " = fmul ";
  auto type = operands_[0]->GetType();
  *out << utils::Operand::GetTypeInLLVMIR(type) << " ";
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

  if (!result_->IsFloat()) {
    context->Print("MulF instruction must have a float result operand\n");
    return false;
  }

  // Verify the operands have exactly 2 operands, and both are float
  if (operands_.size() != 2) {
    context->Print("MulF instruction must have exactly 2 operands\n");
    return false;
  }
  if (operands_[0] == nullptr || operands_[1] == nullptr) {
    context->Print("MulF instruction operands cannot be null\n");
    return false;
  }
  if (GetParentBlock() == nullptr) {
    context->Print("MulF instruction must have a parent block\n");
    return false;
  }

  if (result_->GetInstruction() != this) {
    context->Print("MulF instruction's result operand must point to itself\n");
    return false;
  }

  return operands_[0]->IsFloat() && operands_[1]->IsFloat();
}

auto MulFInst::Create(context::LLVMIRContext *context, utils::Operand *operand1, utils::Operand *operand2)
    -> MulFInst * {
  auto *inst = new MulFInst();
  auto res = context->GetNewRegOperand(context->GetFloatType());
  inst->SetResult(res);
  res->SetInstruction(inst);
  inst->AddOperand(operand1);
  inst->AddOperand(operand2);
  return inst;
}

// Div
auto DivInst::Emit(std::ofstream *out) const -> void {
  *out << "  ";
  result_->Emit(out);
  *out << " = sdiv ";
  auto type = operands_[0]->GetType();
  *out << utils::Operand::GetTypeInLLVMIR(type) << " ";
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

  if (!result_->IsInt32()) {
    context->Print("Div instruction must have an int32 result operand\n");
    return false;
  }

  // Verify the operands have exactly 2 operands, and both are int32
  if (operands_.size() != 2) {
    context->Print("Div instruction must have exactly 2 operands\n");
    return false;
  }
  if (operands_[0] == nullptr || operands_[1] == nullptr) {
    context->Print("Div instruction operands cannot be null\n");
    return false;
  }
  if (GetParentBlock() == nullptr) {
    context->Print("Div instruction must have a parent block\n");
    return false;
  }

  if (result_->GetInstruction() != this) {
    context->Print("Div instruction's result operand must point to itself\n");
    return false;
  }

  return operands_[0]->IsInt32() && operands_[1]->IsInt32();
}

auto DivInst::Create(context::LLVMIRContext *context, utils::Operand *operand1, utils::Operand *operand2) -> DivInst * {
  auto *inst = new DivInst();
  auto res = context->GetNewRegOperand(context->GetIntegerType());
  inst->SetResult(res);
  res->SetInstruction(inst);
  inst->AddOperand(operand1);
  inst->AddOperand(operand2);
  return inst;
}

// Float div
auto DivFInst::Emit(std::ofstream *out) const -> void {
  *out << "  ";
  result_->Emit(out);
  *out << " = fdiv ";
  auto type = operands_[0]->GetType();
  *out << utils::Operand::GetTypeInLLVMIR(type) << " ";
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

  if (!result_->IsFloat()) {
    context->Print("DivF instruction must have a float result operand\n");
    return false;
  }

  // Verify the operands have exactly 2 operands, and both are float
  if (operands_.size() != 2) {
    context->Print("DivF instruction must have exactly 2 operands\n");
    return false;
  }
  if (operands_[0] == nullptr || operands_[1] == nullptr) {
    context->Print("DivF instruction operands cannot be null\n");
    return false;
  }
  if (GetParentBlock() == nullptr) {
    context->Print("DivF instruction must have a parent block\n");
    return false;
  }

  if (result_->GetInstruction() != this) {
    context->Print("DivF instruction's result operand must point to itself\n");
    return false;
  }

  return operands_[0]->IsFloat() && operands_[1]->IsFloat();
}

auto DivFInst::Create(context::LLVMIRContext *context, utils::Operand *operand1, utils::Operand *operand2)
    -> DivFInst * {
  auto *inst = new DivFInst();
  auto res = context->GetNewRegOperand(context->GetFloatType());
  inst->SetResult(res);
  res->SetInstruction(inst);
  inst->AddOperand(operand1);
  inst->AddOperand(operand2);
  return inst;
}

/**
 * @brief Mod instruction
 * a little bit different from other arithmetic instructions,
 * because it only supports integer
 */
auto ModInst::Emit(std::ofstream *out) const -> void {
  *out << "  ";
  result_->Emit(out);
  *out << " = srem ";
  auto type = operands_[0]->GetType();
  *out << utils::Operand::GetTypeInLLVMIR(type) << " ";
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
  if (operands_[0] == nullptr || operands_[1] == nullptr) {
    context->Print("Mod instruction operands cannot be null\n");
    return false;
  }
  if (GetParentBlock() == nullptr) {
    context->Print("Mod instruction must have a parent block\n");
    return false;
  }

  if (result_->GetInstruction() != this) {
    context->Print("Mod instruction's result operand must point to itself\n");
    return false;
  }

  return operands_[0]->IsInt32() && operands_[1]->IsInt32();
}

auto ModInst::Create(context::LLVMIRContext *context, utils::Operand *operand1, utils::Operand *operand2) -> ModInst * {
  auto *inst = new ModInst();
  auto res = context->GetNewRegOperand(context->GetIntegerType());
  inst->SetResult(res);
  res->SetInstruction(inst);
  inst->AddOperand(operand1);
  inst->AddOperand(operand2);
  return inst;
}

}  // namespace middleend::llvm::inst
