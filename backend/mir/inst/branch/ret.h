#ifndef BACKEND_MIR_INST_BRANCH_RET_H
#define BACKEND_MIR_INST_BRANCH_RET_H

#include "backend/mir/inst/inst.h"
namespace backend::inst {
class RetInst : public Instruction {
 public:
  auto Verify(context::CompilerContext *context) const -> bool override;

 private:
  explicit RetInst() : Instruction(OpKind::Ret) {}
  static auto Create(context::CompilerContext *context, utils::Operand *ret_value) -> RetInst *;
  friend class utils::factory::InstFactory;
};

}  // namespace backend::inst

#endif