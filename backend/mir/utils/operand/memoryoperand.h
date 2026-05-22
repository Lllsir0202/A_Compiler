#ifndef BACKEND_MIR_UTILS_OPERAND_MEMORYOPERAND_H
#define BACKEND_MIR_UTILS_OPERAND_MEMORYOPERAND_H

#include "backend/mir/utils/operand/operand.h"
namespace backend::utils {
enum class MemSize : uint8_t { UnSet, Bit32, Bit64 };
/**
 * @brief This class represents the memory operand in the virtual mir,
 * which will be used as the result of the memory-related instructions,
 * and we will set the operand's actual memory size while lowering.
 */
class MemoryOperand : public Operand {
 public:
  /**
   * @brief Interface to access the parameter information, which will be used in optimizing the register allocation for
   * parameters.
   */
  auto IsParameter() const -> bool { return is_parameter_; }
  auto SetIsParameter(bool is_parameter) -> void { is_parameter_ = is_parameter; }
  auto GetParameterIndex() const -> int { return parameter_index_; }
  auto SetParameterIndex(int parameter_index) -> void { parameter_index_ = parameter_index; }

  /**
   * @brief Since the memory address will vary among different architectures,
   * we can set the memory size of the memory operand and deal with them separately in the lowering phase.
   */
  auto GetMemSize() const -> MemSize { return mem_size_; }
  auto SetMemSize(MemSize mem_size) -> void { mem_size_ = mem_size; }

 private:
  explicit MemoryOperand(size_t reg_num) : Operand(OpType::Memory), reg_num_(reg_num) {}
  // I would like to use the memory operand as a special register operand
  size_t reg_num_;
  // Memory operand might be used as a parameter as well
  bool is_parameter_ = false;
  // Parameter index
  int parameter_index_ = -1;
  // It will never be used as a return value, since we do not allow pointer return value

  friend class factory::OperandFactory;
  MemSize mem_size_ = MemSize::UnSet;
};

}  // namespace backend::utils

#endif