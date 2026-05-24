#ifndef CONTEXT_MIR_CONTEXT_H
#define CONTEXT_MIR_CONTEXT_H
#include <unordered_map>
#include <vector>
#include "backend/mir/utils/factory/opfactory.h"

namespace context {
class MIRResourceContext {
 public:
  ~MIRResourceContext();
  // Interfaces to create the operands
  auto GetNewRegOperand(size_t reg_num, type::Type *type) -> backend::utils::Operand *;
  auto GetRegOperand(size_t reg_num) -> backend::utils::Operand *;
  auto GetIntLiteral(int value) -> backend::utils::Operand *;
  auto GetFloatLiteral(float value) -> backend::utils::Operand *;
  auto GetLabelOperand(size_t id) -> backend::utils::Operand *;
  auto GetGlobalOperand(const std::string &name) -> backend::utils::Operand *;
  auto GetBoolLiteral(bool value) -> backend::utils::Operand *;
  auto GetLongLiteral(std::int64_t value) -> backend::utils::Operand *;
  auto GetCharLiteral(char value) -> backend::utils::Operand *;
  auto GetMemoryOperand(size_t reg_num) -> backend::utils::Operand *;
  auto GetNewMemoryOperand(size_t reg_num, type::Type *type) -> backend::utils::Operand *;

  auto Reset() {
    label_operands_.clear();
    reg_operands_.clear();
    int_literal_operands_.clear();
    float_literal_operands_.clear();
    bool_literal_operands_.clear();
    long_literal_operands_.clear();
    char_literal_operands_.clear();
    memory_operands_.clear();
    reg_num_counter_ = 0;
  }

 private:
  // Used as a counter to generate new register operands,
  // and it will be reset for each function
  size_t reg_num_counter_ = 0;

  // Map from id or value to operand
  std::unordered_map<size_t, backend::utils::LabelOperand *> label_operands_;
  std::unordered_map<size_t, backend::utils::RegOperand *> reg_operands_;
  std::unordered_map<std::string, backend::utils::GlobalOperand *> global_operands_;
  std::unordered_map<int, backend::utils::IntLiteral *> int_literal_operands_;
  std::unordered_map<float, backend::utils::FloatLiteral *> float_literal_operands_;
  std::unordered_map<bool, backend::utils::BoolLiteral *> bool_literal_operands_;
  std::unordered_map<std::int64_t, backend::utils::LongLiteral *> long_literal_operands_;
  std::unordered_map<char, backend::utils::CharLiteral *> char_literal_operands_;
  std::unordered_map<size_t, backend::utils::MemoryOperand *> memory_operands_;

  // Record all instructions and free them when the context is destroyed
  std::vector<backend::inst::Instruction *> instructions_;

  // A list of all operands, which will be freed when the context is destroyed
  std::vector<backend::utils::Operand *> operands_;
};

/**
 * @brief The MIRContext class is responsible for
 * managing the state and information related to the MIR.
 * To be honest, it is really really similar to the LLVMIRContext,
 * however, this phase is used as a bridge between the middleend and backend,
 * I hope it can be more clear and decoupled for multiple backends
 */
class MIRContext {
  // Resource context

 public:
  // Interfaces to get some resources
  auto GetNewRegOperand(size_t reg_num, type::Type *type) -> backend::utils::Operand * {
    return resource_context_->GetNewRegOperand(reg_num, type);
  }

  auto GetRegOperand(size_t reg_num) -> backend::utils::Operand * { return resource_context_->GetRegOperand(reg_num); }
  auto GetIntLiteral(int value) -> backend::utils::Operand * { return resource_context_->GetIntLiteral(value); }
  auto GetFloatLiteral(float value) -> backend::utils::Operand * { return resource_context_->GetFloatLiteral(value); }
  auto GetLabelOperand(size_t id) -> backend::utils::Operand * { return resource_context_->GetLabelOperand(id); }

 private:
  MIRResourceContext *resource_context_;
};

}  // namespace context

#endif