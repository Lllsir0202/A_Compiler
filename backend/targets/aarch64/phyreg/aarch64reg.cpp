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
  // Register class first determines the classes of registers
  switch (reg_class) {
    case TargetRegClass::General: {
      // For armv8, x0-x30 for 64-bit, w0-w30 for 32-bit
      // Currently we only allow 32-bit and 64-bit
      if (reg_size == TargetRegSize::DWord) {
        return "w" + std::to_string(id);
      }
      if (reg_size == TargetRegSize::QWord) {
        return "x" + std::to_string(id);
      }
    } break;
    case TargetRegClass::Float: {
      // In armv8, s0-s31 for 32-bit, d0-d31 for 64-bit
      if (reg_size == TargetRegSize::DWord) {
        return "s" + std::to_string(id);
      }
      if (reg_size == TargetRegSize::QWord) {
        return "d" + std::to_string(id);
      }
    } break;
    case TargetRegClass::Special: {
      // For special registers, we can handle them separately based on the id
      // We set zero register and stack pointer as special registers, however,
      // in hardware, they are both gpr-31, but we can treat them as TWO different registers in our backend.
      // BUT NOTE THAT, indeed they are the same register but in different instructions,
      // they have different semantic meanings.
      if (id == 0) {
        return "xzr";  // zero register
      }
      if (id == 1) {
        return "sp";  // stack pointer
      }
      if (id == 2) {
        return "fp";  // frame pointer
      }
      if (id == 3) {
        return "lr";  // link register
      }
      // Done, currently we only have 4 special registers
    } break;
    case TargetRegClass::Vector: {
      // Not handled for now, we will handle it when we support SVE
    } break;
    default:
      break;
  }
  return "unknown_reg";
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