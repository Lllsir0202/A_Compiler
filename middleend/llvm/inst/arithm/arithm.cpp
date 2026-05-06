#include "middleend/llvm/inst/arithm/arithm.h"
#include <fstream>
#include "middleend/llvm/utils/operand/operand.h"

namespace middleend::llvm::inst {
auto AddInst::Emit(std::ofstream *out) const -> void {
  /**
   * @brief Emit the add instruction as a string
   * @note The format of the add instruction is:
   * %result = add i32 %op1, %op2
   * where %result is the result operand, i32 is the type of the operands,
   */

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
}

auto AddInst::Verify() const -> bool {
  // Verify the result operand is a register operand
  if (result_ == nullptr || !result_->IsRegister()) {
    return false;
  }
  // Verify the operands have exactly 2 operands, and both are int32
  if (operands_.size() != 2) {
    return false;
  }

  return operands_[0]->IsInt32() && operands_[1]->IsInt32();
}

// Float add
auto AddFInst::Emit(std::ofstream *out) const -> void {
  result_->Emit(out);
  *out << " = fadd ";

  auto type = operands_[0]->GetType();
  *out << utils::Operand::GetTypeInLLVMIR(type) << " ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
}

auto AddFInst::Verify() const -> bool {
  // Verify the result operand is a register operand
  if (result_ == nullptr || !result_->IsRegister()) {
    return false;
  }
  // Verify the operands have exactly 2 operands, and both are float
  if (operands_.size() != 2) {
    return false;
  }

  return operands_[0]->IsFloat() && operands_[1]->IsFloat();
}

auto SubInst::Emit(std::ofstream *out) const -> void {
  /**
   * @brief Emit the sub instruction
   */
  result_->Emit(out);
  *out << " = sub ";
  // Has checked the operand size and type
  auto type = operands_[0]->GetType();
  *out << utils::Operand::GetTypeInLLVMIR(type) << " ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
}

auto SubInst::Verify() const -> bool {
  /**
   * @brief Verify the sub instruction
   * 1. The operand size should be 2
   * 2. The operand type should be the same and either int or float
   * 3. The result type should be the same as the operand type
   */
  // Verify the result operand is a register operand
  if (result_ == nullptr || !result_->IsRegister()) {
    return false;
  }
  // Verify the operands have exactly 2 operands, and both are int32
  if (operands_.size() != 2) {
    return false;
  }

  return operands_[0]->IsInt32() && operands_[1]->IsInt32();
}

// Float sub
auto SubFInst::Emit(std::ofstream *out) const -> void {
  result_->Emit(out);
  *out << " = fsub ";

  auto type = operands_[0]->GetType();
  *out << utils::Operand::GetTypeInLLVMIR(type) << " ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
}

auto SubFInst::Verify() const -> bool {
  // Verify the result operand is a register operand
  if (result_ == nullptr || !result_->IsRegister()) {
    return false;
  }
  // Verify the operands have exactly 2 operands, and both are float
  if (operands_.size() != 2) {
    return false;
  }

  return operands_[0]->IsFloat() && operands_[1]->IsFloat();
}

// Mul
auto MulInst::Emit(std::ofstream *out) const -> void {
  result_->Emit(out);
  *out << " = mul ";

  auto type = operands_[0]->GetType();
  *out << utils::Operand::GetTypeInLLVMIR(type) << " ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
}

auto MulInst::Verify() const -> bool {
  // Verify the result operand is a register operand
  if (result_ == nullptr || !result_->IsRegister()) {
    return false;
  }
  // Verify the operands have exactly 2 operands, and both are int32
  if (operands_.size() != 2) {
    return false;
  }

  return operands_[0]->IsInt32() && operands_[1]->IsInt32();
}

// Float mul
auto MulFInst::Emit(std::ofstream *out) const -> void {
  result_->Emit(out);
  *out << " = fmul ";
  auto type = operands_[0]->GetType();
  *out << utils::Operand::GetTypeInLLVMIR(type) << " ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
}

auto MulFInst::Verify() const -> bool {
  // Verify the result operand is a register operand
  if (result_ == nullptr || !result_->IsRegister()) {
    return false;
  }
  // Verify the operands have exactly 2 operands, and both are float
  if (operands_.size() != 2) {
    return false;
  }

  return operands_[0]->IsFloat() && operands_[1]->IsFloat();
}

// Div
auto DivInst::Emit(std::ofstream *out) const -> void {
  result_->Emit(out);
  *out << " = sdiv ";
  auto type = operands_[0]->GetType();
  *out << utils::Operand::GetTypeInLLVMIR(type) << " ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
}

// Float div
auto DivFInst::Emit(std::ofstream *out) const -> void {
  result_->Emit(out);
  *out << " = fdiv ";
  auto type = operands_[0]->GetType();
  *out << utils::Operand::GetTypeInLLVMIR(type) << " ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
}

/**
 * @brief Mod instruction
 * a little bit different from other arithmetic instructions,
 * because it only supports integer
 */
auto ModInst::Emit(std::ofstream *out) const -> void {
  result_->Emit(out);
  *out << " = srem ";
  auto type = operands_[0]->GetType();
  *out << utils::Operand::GetTypeInLLVMIR(type) << " ";
  operands_[0]->Emit(out);
  *out << ", ";
  operands_[1]->Emit(out);
}

auto ModInst::Verify() const -> bool {
  // Verify the result operand is a register operand
  if (result_ == nullptr || !result_->IsRegister()) {
    return false;
  }
  // Verify the operands have exactly 2 operands, and both are int32
  if (operands_.size() != 2) {
    return false;
  }

  return operands_[0]->IsInt32() && operands_[1]->IsInt32();
}

}  // namespace middleend::llvm::inst
