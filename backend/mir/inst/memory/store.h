#ifndef BACKEND_MIR_INST_MEMORY_STORE_H
#define BACKEND_MIR_INST_MEMORY_STORE_H

#include "backend/mir/inst/inst.h"
namespace backend::inst {
class StoreInst : public Instruction {
 public:
  auto Verify(context::CompilerContext *context) const -> bool override;
  auto Emit(std::ofstream *out) const -> void override;
  // Get the pointer operand of the store instruction
  auto GetPtr() const -> utils::Operand * { return operands_[1]; }
  // Get the value operand of the store instruction
  auto GetValue() const -> utils::Operand * { return operands_[0]; }

 private:
  explicit StoreInst() : Instruction(OpKind::Store) {}
  static auto Create(context::MIRContext *context, utils::Operand *value_operand,
                     utils::Operand *ptr_operand) -> StoreInst *;
  friend class utils::factory::InstFactory;
};

}  // namespace backend::inst

#endif