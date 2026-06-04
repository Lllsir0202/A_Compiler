#ifndef BACKEND_MIR_UTILS_OPERAND_GLOABLOPERAND_H
#define BACKEND_MIR_UTILS_OPERAND_GLOABLOPERAND_H
#include "backend/mir/utils/operand/operand.h"

namespace backend::utils {
class GlobalOperand : public Operand {
 public:
  auto Emit(std::ofstream *out) const -> void override {
    // The global operand will be emitted as the global variable name
    *out << "global_" << name_;
  }
  auto GetName() const -> const std::string & { return name_; }

 private:
  explicit GlobalOperand(std::string name) : Operand(OpType::Global), name_(std::move(name)) {}
  std::string name_;
  friend class factory::OperandFactory;
};
}  // namespace backend::utils
#endif