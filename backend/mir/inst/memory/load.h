#ifndef BACKEND_MIR_INST_MEMORY_LOAD_H
#define BACKEND_MIR_INST_MEMORY_LOAD_H

#include "backend/mir/inst/inst.h"
namespace backend::inst {
class LoadInst : public Instruction {
 public:
  auto Verify(context::CompilerContext *context) const -> bool override;

 private:
  explicit LoadInst() : Instruction(OpKind::Load) {}
  static auto Create(context::MIRContext *context, utils::Operand *ptr_operand) -> LoadInst *;
  friend class utils::factory::InstFactory;
};

}  // namespace backend::inst

#endif