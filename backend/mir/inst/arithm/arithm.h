#ifndef BACKEND_MIR_INST_ARITHM_ARITHM_H
#define BACKEND_MIR_INST_ARITHM_ARITHM_H

#include "backend/mir/inst/inst.h"
namespace backend::inst {
#include "backend/mir/inst/arithm/arithm.def"
DEFINE_ARITHM_INST(Add, Add);
DEFINE_ARITHM_INST(Sub, Sub);
DEFINE_ARITHM_INST(Mul, Mul);
DEFINE_ARITHM_INST(Div, Div);
DEFINE_ARITHM_INST(Mod, Mod);

DEFINE_ARITHM_INST(AddF, AddF);
DEFINE_ARITHM_INST(SubF, SubF);
DEFINE_ARITHM_INST(MulF, MulF);
DEFINE_ARITHM_INST(DivF, DivF);

}  // namespace backend::inst

#endif