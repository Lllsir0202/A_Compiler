#ifndef BACKEND_MIR_UTILS_OPERAND_REGOPERAND_H
#define BACKEND_MIR_UTILS_OPERAND_REGOPERAND_H
#include "backend/mir/utils/operand/operand.h"
namespace backend::utils {
enum class RegSize : uint8_t { Bit32, Bit64 };

class RegOperand : public Operand {
 public:
  auto GetRegNum() const -> size_t { return reg_num_; }

  /**
   * @brief Interface to access the parameter and return value information
   */
  auto IsParameter() const -> bool { return is_parameter_; }
  auto SetIsParameter(bool is_parameter) -> void { is_parameter_ = is_parameter; }
  auto GetParameterIndex() const -> int { return parameter_index_; }
  auto SetParameterIndex(int parameter_index) -> void { parameter_index_ = parameter_index; }
  auto IsReturnValue() const -> bool { return is_return_value_; }
  auto SetIsReturnValue(bool is_return_value) -> void { is_return_value_ = is_return_value; }

 private:
  explicit RegOperand(size_t reg_num) : Operand(OpType::Register), reg_num_(reg_num) {}
  size_t reg_num_;
  // Whethet the register is used as a parameter,
  // which will be used in optimizing the register allocation for parameters.
  bool is_parameter_ = false;
  // Parameter index
  int parameter_index_ = -1;
  // Whether the register is used as a return value,
  bool is_return_value_ = false;

  friend class factory::OperandFactory;
};

}  // namespace backend::utils

#endif