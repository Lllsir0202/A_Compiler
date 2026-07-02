#ifndef BACKEND_TARGETS_RV64_UTILS_CHECKER_H
#define BACKEND_TARGETS_RV64_UTILS_CHECKER_H

#include "backend/targets/utils/operand/targetoperand.h"
#include "targets/utils/operand/targetreg.h"
namespace backend::targetir::rv64 {

// Used to check if a given integer value can be represented as a 12-bit signed integer
// Check in addi, slti, andi, ori, xori, slli, srli, srai instructions
inline auto Is12BitSignedInteger(int value) -> bool { return value >= -2048 && value <= 2047; }

// Used to check if a given integer value can be represented as a 20-bit signed integer
// Check in lui or auipc instruction
inline auto Is20BitSignedInteger(int value) -> bool { return value >= -524288 && value <= 524287; }

// Check if a given physical register is zero-register
inline auto IsZeroRegister(backend::targetir::TargetOperand *operand) -> bool {
  auto preg = dynamic_cast<TargetPhysicalReg *>(operand);
  // If it is not a physical register, it cannot be a zero register
  if (preg == nullptr) {
    return false;
  }
  auto reg_class = preg->GetRegClass();
  auto reg_id = preg->GetID();
  return reg_class == TargetRegClass::General && reg_id == 0;
}

}  // namespace backend::targetir::rv64
#endif