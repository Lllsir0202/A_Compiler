#include "backend/targets/rv64/phyreg/rv64reg.h"
#include <vector>
namespace backend::targetir {
static std::vector<std::string> gpr_names = {"zero", "ra", "sp", "gp", "tp",  "t0",  "t1", "t2", "fp", "s1", "a0",
                                             "a1",   "a2", "a3", "a4", "a5",  "a6",  "a7", "s2", "s3", "s4", "s5",
                                             "s6",   "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"};
static std::vector<std::string> fpr_names = {
    "ft0", "ft1", "ft2", "ft3", "ft4", "ft5", "ft6", "ft7", "fs0", "fs1", "fa0",  "fa1",  "fa2", "fa3", "fa4",  "fa5",
    "fa6", "fa7", "fs2", "fs3", "fs4", "fs5", "fs6", "fs7", "fs8", "fs9", "fs10", "fs11", "ft8", "ft9", "ft10", "ft11"};

auto RV64RegPrinter::GetRegName([[maybe_unused]] TargetRegSize reg_size, uint8_t id, TargetRegClass reg_class,
                                [[maybe_unused]] TargetRegRole role) const -> std::string {
  switch (reg_class) {
    case TargetRegClass::General: {
      return gpr_names[id];
    }
    case TargetRegClass::Float: {
      return fpr_names[id];
    }
    case TargetRegClass::Vector: {
      // RV64 does not have vector registers, should report error
      throw std::runtime_error("RV64 does not have vector registers");
    }
    default:
      throw std::runtime_error("Invalid register class");
  }
  return "unknown_reg";
}
}  // namespace backend::targetir