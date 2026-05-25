#ifndef BACKEND_MIR_INST_PSEUDO_COPY_H
#define BACKEND_MIR_INST_PSEUDO_COPY_H

#include "backend/mir/inst/inst.h"

namespace backend::inst {
class CopyInst : public Instruction {
 public:
  auto Verify(context::CompilerContext *context) const -> bool override;
  auto GetSrc() const -> utils::Operand * { return operands_[0]; }

 private:
  explicit CopyInst() : Instruction(OpKind::Copy) {}
  static auto Create(context::MIRContext *context, utils::Operand *src) -> CopyInst *;
  friend class utils::factory::InstFactory;
};

}  // namespace backend::inst

#endif