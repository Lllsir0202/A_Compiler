#ifndef BACKEND_MIR_INST_FUNC_FUNCDECLARE_H
#define BACKEND_MIR_INST_FUNC_FUNCDECLARE_H

#include "backend/mir/inst/inst.h"
namespace backend::inst {
class FuncDeclareInst : public Instruction {
 public:
  auto Verify(context::CompilerContext *context) const -> bool override;

 private:
  explicit FuncDeclareInst() : Instruction(OpKind::FuncDeclare) {}
};

}  // namespace backend::inst

#endif