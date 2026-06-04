#ifndef BACKEND_MIR_INST_ARITHM_ARITHM_H
#define BACKEND_MIR_INST_ARITHM_ARITHM_H

#include <fstream>
#include "backend/mir/inst/inst.h"
#include "mir/utils/operand/operand.h"
namespace backend::inst {
#include "backend/mir/inst/arithm/arithm.def"

DEFINE_ARITHM_MIR_INST(Add, Add);
DEFINE_ARITHM_MIR_INST(Sub, Sub);
DEFINE_ARITHM_MIR_INST(Mul, Mul);
DEFINE_ARITHM_MIR_INST(Div, Div);
DEFINE_ARITHM_MIR_INST(Mod, Mod);

DEFINE_ARITHM_MIR_INST(AddF, AddF);
DEFINE_ARITHM_MIR_INST(SubF, SubF);
DEFINE_ARITHM_MIR_INST(MulF, MulF);
DEFINE_ARITHM_MIR_INST(DivF, DivF);

}  // namespace backend::inst

#endif