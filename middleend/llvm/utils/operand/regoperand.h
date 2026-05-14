#ifndef MIDDLEEND_LLVM_UTILS_OPERAND_REGOPERAND_H
#define MIDDLEEND_LLVM_UTILS_OPERAND_REGOPERAND_H

#include "middleend/llvm/utils/operand/operand.h"
namespace middleend::llvm::utils {

class RegOperand : public Operand {
 public:
  auto Emit(std::ofstream *out) const -> void override {
    if (name_.empty()) {
      *out << "%" << id_;
    } else {
      *out << "%" << name_;
    }
  }
  // Use for phi
  auto SetName(const std::string &name) -> void { name_ = name; }

 private:
  explicit RegOperand(size_t id) : Operand(OpType::Register), id_(id) {}

  size_t id_;
  // for phi operand result
  std::string name_;
  friend class middleend::llvm::utils::factory::OperandFactory;
};

}  // namespace middleend::llvm::utils

#endif