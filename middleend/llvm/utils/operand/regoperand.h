#ifndef MIDDLEEND_LLVM_UTILS_OPERAND_REGOPERAND_H
#define MIDDLEEND_LLVM_UTILS_OPERAND_REGOPERAND_H

#include "middleend/llvm/utils/operand/operand.h"
namespace middleend::llvm::utils {

class RegOperand : public Operand {
 public:
  auto Emit(std::ofstream *out) const -> void override { *out << "%" << id_; }

 private:
  explicit RegOperand(size_t id) : Operand(OpType::Register), id_(id) {}

  size_t id_;
  friend class middleend::llvm::utils::factory::OperandFactory;
};

}  // namespace middleend::llvm::utils

#endif