#ifndef MIDDLEEND_LLVM_INST_ARITHM_ADD_H
#define MIDDLEEND_LLVM_INST_ARITHM_ADD_H

#include <fstream>
#include "middleend/llvm/inst/inst.h"
namespace middleend::llvm::inst {
#include "middleend/llvm/inst/arithm/arithm.def"

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