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

// Interfaces to create the operands
auto MIRResourceContext::GetNewRegOperand(type::Type *type) -> backend::utils::Operand * {
  auto *reg_operand = backend::utils::factory::OperandFactory::CreateRegOperand(reg_num_counter_++);
  reg_operand->SetType(type);
  operands_.push_back(reg_operand);
  return reg_operand;
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

auto MIRResourceContext::GetNewMemoryOperand() -> backend::utils::Operand * {
  auto *memory_operand = backend::utils::factory::OperandFactory::CreateMemoryOperand();
  operands_.emplace_back(memory_operand);
  memory_operands_.emplace_back(memory_operand);
  return memory_operand;
}

auto MIRResourceContext::GetFrameOperand() -> backend::utils::Operand * {
  /**
   * @note: We should always make sure that the size of frame_operands_
   * is equal to the frame_info's size, so that the slot_id will always be the index
   * of the vector
   */
  auto *frame_operand = backend::utils::factory::OperandFactory::CreateFrameOperand(frame_slot_counter_++);
  operands_.emplace_back(frame_operand);
  return frame_operand;
}

// Used in target-specific lowering
auto MIRResourceContext::SetMemorySize(backend::utils::MemSize mem_size) -> void {
  for (auto *memory_operand : memory_operands_) {
    memory_operand->SetMemSize(mem_size);
  }
}

}  // namespace context