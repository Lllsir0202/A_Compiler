#include "context/llvmir_context.h"
#include "llvm/utils/factory/opfactory.h"

namespace context {
auto LLVMIRResourceContext::GetNewRegOperand(type::Type *type) -> middleend::llvm::utils::RegOperand * {
  auto op = middleend::llvm::utils::factory::OperandFactory::CreateRegOperand(reg_id_counter_++);
  op->SetType(type);
  return op;
}

auto LLVMIRResourceContext::GetNewLabelOperand() -> middleend::llvm::utils::LabelOperand * {
  return middleend::llvm::utils::factory::OperandFactory::CreateLabelOperand(label_id_counter_++);
}

auto LLVMIRResourceContext::GetNewIntLiteral(int value, type::Type *type) -> middleend::llvm::utils::IntLiteral * {
  auto op = middleend::llvm::utils::factory::OperandFactory::CreateIntLiteral(value);
  op->SetType(type);
  return op;
}

auto LLVMIRResourceContext::GetNewFloatLiteral(float value,
                                               type::Type *type) -> middleend::llvm::utils::FloatLiteral * {
  auto op = middleend::llvm::utils::factory::OperandFactory::CreateFloatLiteral(value);
  op->SetType(type);
  return op;
}

// Get existing operands by id(or value for literals)
auto LLVMIRResourceContext::GetRegOperand(size_t id) -> middleend::llvm::utils::RegOperand * {
  if (reg_operands_.count(id) > 0) {
    return reg_operands_[id];
  }
  // If the operand does not exist, report error
  return nullptr;
}

auto LLVMIRResourceContext::GetLabelOperand(size_t id) -> middleend::llvm::utils::LabelOperand * {
  if (label_operands_.count(id) > 0) {
    return label_operands_[id];
  }
  // If the operand does not exist, report error
  return nullptr;
}

auto LLVMIRResourceContext::GetIntLiteral(int value) -> middleend::llvm::utils::IntLiteral * {
  if (int_literals_.count(value) > 0) {
    return int_literals_[value];
  }
  // If the operand does not exist, report error
  return nullptr;
}

auto LLVMIRResourceContext::GetFloatLiteral(float value) -> middleend::llvm::utils::FloatLiteral * {
  if (float_literals_.count(value) > 0) {
    return float_literals_[value];
  }
  // If the operand does not exist, report error
  return nullptr;
}

}  // namespace context