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
auto AArch64RegPrinter::GetRegName(TargetRegSize reg_size, uint8_t id, TargetRegClass reg_class,
                                   TargetRegRole role) const -> std::string {
  // For GPRs (General Purpose Registers):
  switch (reg_class) {
    case TargetRegClass::General: {
      // GPRs: x0-x28 general use, x29 is fp, x30 is lr, x31 is sp or zr
      if (id <= 30) {
        if (reg_size == TargetRegSize::Word) {
          return "w" + std::to_string(id);
        }
        if (reg_size == TargetRegSize::QWord) {
          return "x" + std::to_string(id);
        }
      }
      if (id == 31) {
        if (role == TargetRegRole::STACKPOINTER) {
          return "sp";
        }
        if (role == TargetRegRole::ZERO) {
          if (reg_size == TargetRegSize::Word) {
            return "wzr";
          }
          if (reg_size == TargetRegSize::QWord) {
            return "xzr";
          }
        }
      }
    } break;
    // For FPRs (Floating Point Registers):
    // 32-bit: s0-s31 (e.g., s0, s1, ..., s31)
    // 64-bit: d0-d31 (e.g., d0, d1, ..., d31)
    case TargetRegClass::Float: {
      if (reg_size == TargetRegSize::DWord) {
        return "s" + std::to_string(id);
      }
      if (reg_size == TargetRegSize::QWord) {
        return "d" + std::to_string(id);
      }
    } break;
    case TargetRegClass::Vector: {
      // Handle in the future when SVE is supported
    } break;
    default:
      throw std::runtime_error("Invalid register class");
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