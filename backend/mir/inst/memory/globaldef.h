#ifndef BACKEND_MIR_INST_MEMORY_GLOBALDEF_H
#define BACKEND_MIR_INST_MEMORY_GLOBALDEF_H

#include "backend/mir/inst/inst.h"
namespace backend::inst {
class GlobalDefInst : public Instruction {
 public:
  auto Verify(context::CompilerContext *context) const -> bool override;

 private:
  explicit GlobalDefInst() : Instruction(OpKind::GlobalDef) {}
};

}  // namespace backend::inst

#endif