#ifndef BACKEND_MIR_UTILS_OPERAND_OPERAND_H
#define BACKEND_MIR_UTILS_OPERAND_OPERAND_H

#include <cstdint>
#include <fstream>
#include "utils/type/type.h"
namespace backend::utils::factory {
class OperandFactory;
}

namespace backend::inst {
class Instruction;
}

namespace backend::utils {

/**
 * @brief This class represents the operand in the virtual machine instruction,
 * which can be a register, a literal, a memory address(maybe unused), a label, a global variable or a frame operand.
 * To be more detailed,
 * 1. a memory operand will record following information:
 * base pointer(or stack slot), index operands(or immeidaite), offset
 * 2. a frame operand will record the stack slot id, the slot id will be able to assist
 * getting access to the stack-related information
 */

// Memory operand will be used as a pointer to the memory address,
// and the actual memory address will be calculated in lowering.
enum class OpType : uint8_t { Register, Literal, Memory, Label, Global, Frame };

class Operand {
 public:
  virtual auto Emit(std::ofstream *out) const -> void = 0;

  auto GetOpType() const -> OpType { return op_type_; }
  auto GetType() const -> type::Type * { return type_; }
  auto SetType(type::Type *type) -> void { type_ = type; }

  auto GetDefInst() const -> inst::Instruction * { return def_inst_; }
  auto SetDefInst(inst::Instruction *def_inst) -> void { def_inst_ = def_inst; }

  // Interface to check the optype of the operand
  auto IsRegister() const -> bool { return op_type_ == OpType::Register; }
  auto IsLiteral() const -> bool { return op_type_ == OpType::Literal; }
  auto IsMemory() const -> bool { return op_type_ == OpType::Memory; }
  auto IsLabel() const -> bool { return op_type_ == OpType::Label; }
  auto IsGlobal() const -> bool { return op_type_ == OpType::Global; }
  auto IsFrame() const -> bool { return op_type_ == OpType::Frame; }
  // Interface to check the type of the operand
  auto IsInt() const -> bool { return type_ != nullptr && type_->GetKind() == type::TypeKind::INT; }
  auto IsLong() const -> bool { return type_ != nullptr && type_->GetKind() == type::TypeKind::LONG; }
  auto IsFloat() const -> bool { return type_ != nullptr && type_->GetKind() == type::TypeKind::FLOAT; }
  auto IsBool() const -> bool { return type_ != nullptr && type_->GetKind() == type::TypeKind::BOOL; }
  auto IsVoid() const -> bool { return type_ != nullptr && type_->GetKind() == type::TypeKind::VOID; }
  auto IsPtr() const -> bool { return type_ != nullptr && type_->GetKind() == type::TypeKind::PTR; }

  // Verify the optype
  auto VerifyOpKind(OpType expected_op_type) const -> bool { return op_type_ == expected_op_type; }

  // Record if it is used as a parameter
  /**
   * @brief Interface to access the parameter and return value information
   */
  auto IsParameter() const -> bool { return is_parameter_; }
  auto SetIsParameter(bool is_parameter) -> void { is_parameter_ = is_parameter; }
  auto GetIndex() const -> size_t { return index_; }
  auto SetIndex(size_t index) -> void { index_ = index; }
  auto IsReturnValue() const -> bool { return is_return_value_; }
  auto SetIsReturnValue(bool is_return_value) -> void { is_return_value_ = is_return_value; }

  virtual ~Operand() = default;

 protected:
  explicit Operand(OpType op_type) : op_type_(op_type) {}

 private:
  const OpType op_type_;
  /**
   * @note: In the backend, we need to consider operand size and type,
   * and we need to be extremely careful about the 64-bit register and 32-bit register,
   * since they can be exactly a same register in physical register allocation, so that
   * we need to make sure that the same physical register can be recognized as the same in allocation.
   */
  type::Type *type_ = nullptr;
  friend class factory::OperandFactory;
  // Record the instruction that defines the operand
  inst::Instruction *def_inst_ = nullptr;

  // Record whether it is used as a parameter
  bool is_parameter_ = false;
  size_t index_ = 0;
  // Record whether it is used as a return value
  bool is_return_value_ = false;
};
}  // namespace backend::utils

#endif
