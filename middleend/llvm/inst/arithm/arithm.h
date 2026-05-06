#ifndef MIDDLEEND_LLVM_INST_ARITHM_ADD_H
#define MIDDLEEND_LLVM_INST_ARITHM_ADD_H

#include <fstream>
#include "llvm/utils/operand/operand.h"
#include "middleend/llvm/inst/inst.h"

namespace middleend::llvm::inst {
// #include "middleend/llvm/inst/arithm/arithm.def"
/**
 * @brief Defines an arithmetic instruction with the given name and operation kind.
 */
#define DEFINE_ARITHM_INST(NAME, OPKIND)                                                                            \
  class NAME##Inst : public Instruction {                                                                           \
   public:                                                                                                          \
    auto Emit(std::ofstream *out) const -> void override;                                                           \
    auto Verify(context::CompilerContext *context) const -> bool override;                                          \
    ~NAME##Inst() override = default;                                                                               \
                                                                                                                    \
   private:                                                                                                         \
    /** Only allow the factory to create the instruction */                                                         \
    explicit NAME##Inst() : Instruction(OpKind::OPKIND) {}                                                          \
    static auto Create(context::LLVMIRContext *context, utils::Operand *operand1, utils::Operand *operand2) -> NAME \
        ##Inst *;                                                                                                   \
    friend class utils::factory::InstFactory;                                                                       \
  };

DEFINE_ARITHM_INST(Add, Add)
DEFINE_ARITHM_INST(AddF, AddF)
DEFINE_ARITHM_INST(Sub, Sub)
DEFINE_ARITHM_INST(SubF, SubF)
DEFINE_ARITHM_INST(Mul, Mul)
DEFINE_ARITHM_INST(MulF, MulF)
DEFINE_ARITHM_INST(Div, Div)
DEFINE_ARITHM_INST(DivF, DivF)
DEFINE_ARITHM_INST(Mod, Mod)
}  // namespace middleend::llvm::inst

#endif