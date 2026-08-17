#ifndef BACKEND_TARGETS_RISCV_PRINTER_RISCVPRINTER_H
#define BACKEND_TARGETS_RISCV_PRINTER_RISCVPRINTER_H

#include <string>
#include "backend/targets/riscv/regs/riscvreg.h"
#include "backend/targets/utils/operand/targetoperand.h"
#include "backend/targets/utils/operand/targetprinter.h"

namespace backend::targetir::riscv {

/**
 * @brief Maps (reg id, reg class) to the RISC-V ABI register name.
 *
 * Register size is irrelevant on rv64 for the textual name (x-registers keep the
 * same name regardless of the operation width; the width lives in the mnemonic,
 * e.g. addw vs add), so reg_size is accepted for interface parity but unused.
 */
class RISCVPrinter : public TargetPrinter {
 public:
  auto GetRegName(TargetRegSize /*reg_size*/, uint8_t id, TargetRegClass reg_class) const -> std::string override {
    if (id >= 32) {
      return "<bad-reg>";
    }
    switch (reg_class) {
      case TargetRegClass::Float:
        return FPR_ABI_NAME[id];
      case TargetRegClass::Vector:
        return "v" + std::to_string(id);
      case TargetRegClass::General:
      case TargetRegClass::Special:
      default:
        return GPR_ABI_NAME[id];
    }
  }
};

}  // namespace backend::targetir::riscv

#endif
