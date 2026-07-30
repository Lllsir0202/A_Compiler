#ifndef BACKEND_MIR_INST_ARITHM_WIDEMULMOD_H
#define BACKEND_MIR_INST_ARITHM_WIDEMULMOD_H

#include "backend/mir/inst/inst.h"

namespace backend::inst {

class WideMulModInst : public Instruction {
 public:
  auto Emit(std::ofstream *out) const -> void override;
  auto Verify(context::CompilerContext *context) const -> bool override;

 private:
  WideMulModInst() : Instruction(OpKind::WideMulMod) {}
  static auto Create(context::MIRContext *context, utils::Operand *lhs, utils::Operand *rhs,
                     utils::Operand *modulus) -> WideMulModInst *;
  friend class utils::factory::InstFactory;
};

}  // namespace backend::inst

#endif
