#ifndef MIDDLEEND_LLVM_UTILS_OPERAND_LABELOPERAND_H
#define MIDDLEEND_LLVM_UTILS_OPERAND_LABELOPERAND_H

#include <cstddef>
#include "middleend/llvm/utils/operand/operand.h"

namespace middleend::llvm::utils::operand {

class LabelOperand : public Operand {
 public:
  auto Emit() const -> std::string override { return "label" + std::to_string(id_); }

 private:
  explicit LabelOperand(size_t id) : Operand(OpType::Label), id_(id) {}

  size_t id_;
  friend class middleend::llvm::utils::factory::OperandFactory;
};

}  // namespace middleend::llvm::utils::operand

#endif