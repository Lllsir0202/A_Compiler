#include "backend/targets/aarch64/phyreg/aarch64reg.h"
#include "targets/utils/operand/targetoperand.h"

namespace backend::targetir {
// For aarch64 registers
/**
 * @brief Get the register name based on the register size, id and class
 * @param reg_size The size of the register (e.g., 32-bit, 64-bit)
 * @param id The unique identifier for the register
 * @param reg_class The class of the register (e.g., general-purpose, floating-point)
 * @return The name of the register as a string
 */
auto AArch64RegPrinter::GetRegName(TargetRegSize reg_size, uint8_t id, TargetRegClass reg_class) const -> std::string {
  
}

/**
* @introduce:
* For GPRs (General Purpose Registers):
- 32-bit: W0-W30 (e.g., W0, W1, ..., W30)
- 64-bit: X0-X30 (e.g., X0, X1, ..., X30)
* For FPRs (Floating Point Registers):
- 32-bit: S0-S31 (e.g., S0, S1, ..., S31)
- 64-bit: D0-D31 (e.g., D0, D1, ..., D31)
* For SVE (Scalable Vector Extension) registers:
* Handle in the future when SVE is supported
*/

}  // namespace backend::targetir