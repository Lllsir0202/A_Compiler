#ifndef BACKEND_MIR_INST_FUNC_FUNCDEF_H
#define BACKEND_MIR_INST_FUNC_FUNCDEF_H

#include "backend/mir/inst/inst.h"
namespace backend::inst {
class FuncDefInst : public Instruction {
 public:
  auto Verify(context::CompilerContext *context) const -> bool override;

 private:
  explicit FuncDefInst() : Instruction(OpKind::FuncDef) {}
};

}  // namespace backend::inst

#endif