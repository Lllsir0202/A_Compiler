#ifndef MIDDLEEND_LLVM_UTILS_OPERAND_GLOBALOPERAND_H
#define MIDDLEEND_LLVM_UTILS_OPERAND_GLOBALOPERAND_H
#include "middleend/llvm/utils/operand/operand.h"
namespace middleend::llvm::utils {
class GlobalOperand : public Operand {
 public:
  auto GetName() const -> const std::string & { return name_; }
  auto Emit(std::ofstream *out) const -> void override { *out << "@" << name_; }

 private:
  explicit GlobalOperand(std::string name) : Operand(OpType::Global), name_(std::move(name)) {}
  friend class factory::OperandFactory;

  std::string name_;
};

}  // namespace middleend::llvm::utils

#endif