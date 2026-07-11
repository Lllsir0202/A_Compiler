#include "backend/targets/utils/operand/targetreg.h"

namespace backend::targetir {

// Definition of the static printer shared by all physical registers; installed
// by the target backend before emission (see RISCVBackend::Emit).
const TargetPrinter *TargetPhysicalReg::target_printer = nullptr;

}  // namespace backend::targetir
