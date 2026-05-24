#include "context/mir_context.h"
#include <cstddef>
#include "backend/mir/inst/inst.h"

namespace context {
MIRResourceContext::~MIRResourceContext() {
  for (auto *inst : instructions_) {
    delete inst;
  }
  for (auto *operand : operands_) {
    delete operand;
  }
}

auto MIRResourceContext::GetNewRegOperand(size_t reg_num, type::Type *type) -> backend::utils::Operand * {
  auto *reg_operand = backend::utils::factory::OperandFactory::CreateRegOperand(reg_num);
  reg_operand->SetType(type);
  operands_.emplace_back(reg_operand);
  reg_operands_[reg_num] = reg_operand;
  return reg_operand;
}

auto MIRResourceContext::GetRegOperand(size_t reg_num) -> backend::utils::Operand * {
  if (reg_operands_.count(reg_num) > 0) {
    return reg_operands_[reg_num];
  }
  // Do not allow to get a register operand that has not been created, report error
  return nullptr;
}

auto MIRResourceContext::GetIntLiteral(int value) -> backend::utils::Operand * {
  if (int_literal_operands_.count(value) > 0) {
    return int_literal_operands_[value];
  }
  auto *int_literal = backend::utils::factory::OperandFactory::CreateIntLiteral(value);
  operands_.emplace_back(int_literal);
  int_literal_operands_[value] = int_literal;
  return int_literal;
}

auto MIRResourceContext::GetFloatLiteral(float value) -> backend::utils::Operand * {
  if (float_literal_operands_.count(value) > 0) {
    return float_literal_operands_[value];
  }
  auto *float_literal = backend::utils::factory::OperandFactory::CreateFloatLiteral(value);
  operands_.emplace_back(float_literal);
  float_literal_operands_[value] = float_literal;
  return float_literal;
}

auto MIRResourceContext::GetLabelOperand(size_t id) -> backend::utils::Operand * {
  if (label_operands_.count(id) > 0) {
    return label_operands_[id];
  }
  auto *label_operand = backend::utils::factory::OperandFactory::CreateLabelOperand(id);
  operands_.emplace_back(label_operand);
  label_operands_[id] = label_operand;
  return label_operand;
}

auto MIRResourceContext::GetGlobalOperand(const std::string &name) -> backend::utils::Operand * {
  if (global_operands_.count(name) > 0) {
    return global_operands_[name];
  }
  auto *global_operand = backend::utils::factory::OperandFactory::CreateGlobalOperand(name);
  operands_.emplace_back(global_operand);
  global_operands_[name] = global_operand;
  return global_operand;
}

auto MIRResourceContext::GetBoolLiteral(bool value) -> backend::utils::Operand * {
  if (bool_literal_operands_.count(value) > 0) {
    return bool_literal_operands_[value];
  }
  auto *bool_literal = backend::utils::factory::OperandFactory::CreateBoolLiteral(value);
  operands_.emplace_back(bool_literal);
  bool_literal_operands_[value] = bool_literal;
  return bool_literal;
}

auto MIRResourceContext::GetCharLiteral(char value) -> backend::utils::Operand * {
  if (char_literal_operands_.count(value) > 0) {
    return char_literal_operands_[value];
  }
  auto *char_literal = backend::utils::factory::OperandFactory::CreateCharLiteral(value);
  operands_.emplace_back(char_literal);
  char_literal_operands_[value] = char_literal;
  return char_literal;
}

auto MIRResourceContext::GetLongLiteral(std::int64_t value) -> backend::utils::Operand * {
  if (long_literal_operands_.count(value) > 0) {
    return long_literal_operands_[value];
  }
  auto *long_literal = backend::utils::factory::OperandFactory::CreateLongLiteral(value);
  operands_.emplace_back(long_literal);
  long_literal_operands_[value] = long_literal;
  return long_literal;
}

auto MIRResourceContext::GetMemoryOperand(size_t reg_num) -> backend::utils::Operand * {
  if (memory_operands_.count(reg_num) > 0) {
    return memory_operands_[reg_num];
  }
  // Do not allow directly get a memory operand that has not been created, report error
  return nullptr;
}

auto MIRResourceContext::GetNewMemoryOperand(size_t reg_num, type::Type *type) -> backend::utils::Operand * {
  if (memory_operands_.count(reg_num) > 0) {
    return memory_operands_[reg_num];
  }
  auto *memory_operand = backend::utils::factory::OperandFactory::CreateMemoryOperand(reg_num);
  memory_operand->SetType(type);
  operands_.emplace_back(memory_operand);
  memory_operands_[reg_num] = memory_operand;
  return memory_operand;
}

}  // namespace context