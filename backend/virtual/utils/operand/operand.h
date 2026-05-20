#ifndef BACKEND_VIRTUAL_UTILS_OPERAND_OPERAND_H_
#define BACKEND_VIRTUAL_UTILS_OPERAND_OPERAND_H_

#include <cstdint>
namespace backend::utils {
/**
 * @brief This class represents the operand in the virtual machine instruction,
 * which can be a register, a literal, a memory address(maybe unused), a label or a global variable.
 */
enum class OpType : uint8_t { Register, Literal, Memory, Label, Global };

class Operand {
 public:
  auto GetOpType() const -> OpType { return op_type_; }

 protected:
  explicit Operand(OpType op_type) : op_type_(op_type) {}

 private:
  const OpType op_type_;
};
}  // namespace backend::utils

#endif