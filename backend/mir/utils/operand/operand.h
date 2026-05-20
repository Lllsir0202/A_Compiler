#ifndef BACKEND_MIR_UTILS_OPERAND_OPERAND_H_
#define BACKEND_MIR_UTILS_OPERAND_OPERAND_H_

#include <cstdint>
#include "utils/type/type.h"
namespace backend::utils {
/**
 * @brief This class represents the operand in the virtual machine instruction,
 * which can be a register, a literal, a memory address(maybe unused), a label or a global variable.
 */

// Memory operand will be used as a pointer to the memory address,
// and the actual memory address will be calculated in lowering.
enum class OpType : uint8_t { Register, Literal, Memory, Label, Global };

class Operand {
 public:
  auto GetOpType() const -> OpType { return op_type_; }
  auto GetType() const -> type::Type * { return type_; }
  auto SetType(type::Type *type) -> void { type_ = type; }

  // Interface to check the optype of the operand
  auto IsRegister() const -> bool { return op_type_ == OpType::Register; }
  auto IsLiteral() const -> bool { return op_type_ == OpType::Literal; }
  auto IsMemory() const -> bool { return op_type_ == OpType::Memory; }
  auto IsLabel() const -> bool { return op_type_ == OpType::Label; }
  auto IsGlobal() const -> bool { return op_type_ == OpType::Global; }

  // Interface to check the type of the operand
  auto IsInt() const -> bool { return type_ != nullptr && type_->GetKind() == type::TypeKind::INT; }
  auto IsFloat() const -> bool { return type_ != nullptr && type_->GetKind() == type::TypeKind::FLOAT; }
  auto IsBool() const -> bool { return type_ != nullptr && type_->GetKind() == type::TypeKind::BOOL; }
  auto IsVoid() const -> bool { return type_ != nullptr && type_->GetKind() == type::TypeKind::VOID; }
  auto IsPtr() const -> bool { return type_ != nullptr && type_->GetKind() == type::TypeKind::PTR; }

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
};
}  // namespace backend::utils

#endif