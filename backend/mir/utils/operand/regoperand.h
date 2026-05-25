#ifndef BACKEND_MIR_UTILS_OPERAND_REGOPERAND_H
#define BACKEND_MIR_UTILS_OPERAND_REGOPERAND_H
#include "backend/mir/utils/operand/operand.h"
namespace backend::utils {
enum class RegSize : uint8_t { Bit32, Bit64 };

class RegOperand : public Operand {
 public:
  auto GetRegNum() const -> size_t { return reg_num_; }

 private:
  explicit RegOperand(size_t reg_num) : Operand(OpType::Register), reg_num_(reg_num) {}
  size_t reg_num_;

  friend class factory::OperandFactory;
};

}  // namespace backend::utils

#endif