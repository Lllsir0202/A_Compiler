#ifndef BACKEND_TARGETS_AARCH64_PRINTER_AARCH64PRINTER_H
#define BACKEND_TARGETS_AARCH64_PRINTER_AARCH64PRINTER_H

#include <string>
#include "backend/targets/aarch64/regs/aarch64reg.h"
#include "backend/targets/utils/operand/targetoperand.h"
#include "backend/targets/utils/operand/targetprinter.h"

namespace backend::targetir::aarch64 {

/**
 * @brief Maps (reg size, reg id, reg class) to the AArch64 register name.
 *
 * Unlike RISC-V, the width is part of the register NAME (the mnemonic is
 * width-agnostic): a 32-bit GPR is w<id>, a 64-bit GPR is x<id>; a single-
 * precision FPR is s<id>, a double is d<id>. Id 31 of the GPR file is the stack
 * pointer sp / wsp (there is no x31 general register).
 */
class AArch64Printer : public TargetPrinter {
 public:
  auto GetRegName(TargetRegSize reg_size, uint8_t id, TargetRegClass reg_class) const -> std::string override {
    bool is64 = reg_size == TargetRegSize::QWord;
    if (reg_class == TargetRegClass::Float) {
      if (id >= 32) {
        return "<bad-freg>";
      }
      return (is64 ? "d" : "s") + std::to_string(id);
    }
    // General-purpose / special.
    if (id == static_cast<uint8_t>(GPR::Sp)) {
      return is64 ? "sp" : "wsp";
    }
    if (id > static_cast<uint8_t>(GPR::X30)) {
      return "<bad-reg>";
    }
    return (is64 ? "x" : "w") + std::to_string(id);
  }
};

}  // namespace backend::targetir::aarch64

#endif
