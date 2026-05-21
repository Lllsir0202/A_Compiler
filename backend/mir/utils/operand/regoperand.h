#ifndef BACKEND_MIR_UTILS_OPERAND_REGOPERAND_H
#define BACKEND_MIR_UTILS_OPERAND_REGOPERAND_H
#include "backend/mir/utils/operand/operand.h"
namespace backend::utils {
class RegOperand : public Operand {
 public:
  auto GetRegNum() const -> size_t { return reg_num_; }

 private:
  explicit RegOperand(size_t reg_num) : Operand(OpType::Register), reg_num_(reg_num) {}
  size_t reg_num_;
  friend class factory::Operandfactory;
};

}  // namespace backend::utils

#endif