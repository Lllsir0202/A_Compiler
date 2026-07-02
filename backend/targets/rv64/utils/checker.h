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

// Check if it is a power of two, 2^n
inline auto IsPowerOfTwo(int value) -> bool { return value > 0 && (value & (value - 1)) == 0; }

inline auto GetPowerOfTwoExponent(int value) -> int {
  if (!IsPowerOfTwo(value)) {
    throw std::runtime_error("Value is not a power of two");
  }
  int exponent = 0;
  while (value > 1) {
    value >>= 1;
    exponent++;
  }
  return exponent;
}

// Check if it can be expressed as a sum or difference of two powers of two
inline auto IsSumOrDifferenceOfTwoPowersOfTwo(int value) -> bool {
  if (value == 0) {
    return false;
  }
  // Check if it is a power of two
  if (IsPowerOfTwo(value)) {
    return true;
  }
  // Check if it can be expressed as a sum or difference of two powers of two
  for (int i = 0; i < 31; i++) {
    int power_of_two_i = 1 << i;
    for (int j = 0; j < 31; j++) {
      int power_of_two_j = 1 << j;
      if (power_of_two_i + power_of_two_j == value || power_of_two_i - power_of_two_j == value ||
          power_of_two_j - power_of_two_i == value) {
        return true;
      }
    }
  }
  return false;
}

inline auto GetSumOrDifferenceOfTwoPowersOfTwo(int value) -> std::pair<int, int> {
  if (!IsSumOrDifferenceOfTwoPowersOfTwo(value)) {
    throw std::runtime_error("Value is not a sum or difference of two powers of two");
  }
  for (int i = 0; i < 31; i++) {
    int power_of_two_i = 1 << i;
    for (int j = 0; j < 31; j++) {
      int power_of_two_j = 1 << j;
      if (power_of_two_i + power_of_two_j == value || power_of_two_i - power_of_two_j == value ||
          power_of_two_j - power_of_two_i == value) {
        return {i, j};
      }
    }
  }
  throw std::runtime_error("Value is not a sum or difference of two powers of two");
}

}  // namespace backend::targetir::rv64
#endif