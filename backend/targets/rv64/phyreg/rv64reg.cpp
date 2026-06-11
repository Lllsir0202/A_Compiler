#include "backend/targets/rv64/phyreg/rv64reg.h"

namespace backend::targetir {
auto RV64RegPrinter::GetRegName([[maybe_unused]] TargetRegSize reg_size, uint8_t id,
                                TargetRegClass reg_class) const -> std::string {
  // Register class first determines the classes of registers
  switch (reg_class) {
    case TargetRegClass::General: {
      // For rv64, x0-x31 are all registers, but there are some ABI registers
      // We set zero register and stack pointer as special registers
      // Only allow x5-x7, x9-x31, since x8 is s0/fp, it is a special register,
      // we will handle it in the special register case
      if (id >= 5 && id <= 7) {
        return "t" + std::to_string(id - 5);
      }
      if (id == 9) {
        return "s1";
      }
      if (id >= 10 && id <= 17) {
        return "a" + std::to_string(id - 10);
      }
      if (id >= 18 && id <= 27) {
        return "s" + std::to_string(id - 16);
      }
      if (id >= 28 && id <= 31) {
        return "t" + std::to_string(id - 25);
      }
    } break;
    case TargetRegClass::Float: {
      if (id >= 0 && id <= 7) {
        return "ft" + std::to_string(id);
      }
      if (id >= 8 && id <= 9) {
        return "fs" + std::to_string(id - 8);
      }
      if (id >= 10 && id <= 17) {
        return "fa" + std::to_string(id - 10);
      }
      if (id >= 18 && id <= 27) {
        return "fs" + std::to_string(id - 16);
      }
      if (id >= 28 && id <= 31) {
        return "ft" + std::to_string(id - 28);
      }
    } break;
    case TargetRegClass::Special: {
      if (id == 0) {
        return "x0";  // zero register
      }
      if (id == 1) {
        return "sp";  // stack pointer
      }
      if (id == 8) {
        return "fp";  // frame pointer
      }
      // Done, currently we only have 3 special registers
    } break;
    case TargetRegClass::Vector: {
    } break;
    default:
      break;
  }
  return "unknown_reg";
}

}  // namespace backend::targetir