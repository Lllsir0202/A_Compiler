#ifndef MIDDLEEND_LLVM_INST_ARITHM_WIDEMULMOD_H
#define MIDDLEEND_LLVM_INST_ARITHM_WIDEMULMOD_H

#include "middleend/llvm/inst/inst.h"
#include "middleend/llvm/utils/operand/operand.h"

namespace middleend::llvm::inst {

class WideMulModInst : public Instruction {
 public:
  auto Emit(std::ofstream *out) const -> void override;
  auto Verify(context::CompilerContext *context) const -> bool override;
  auto GetLHS() const -> utils::Operand * { return operands_[0]; }
  auto GetRHS() const -> utils::Operand * { return operands_[1]; }
  auto GetModulus() const -> utils::Operand * { return operands_[2]; }

 private:
  WideMulModInst() : Instruction(OpKind::WideMulMod) {}
  static auto Create(context::LLVMIRContext *context, utils::Operand *lhs, utils::Operand *rhs,
                     utils::Operand *modulus) -> WideMulModInst *;
  friend class utils::factory::InstFactory;
};

}  // namespace middleend::llvm::inst

#endif
