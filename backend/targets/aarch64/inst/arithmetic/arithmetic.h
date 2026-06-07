#ifndef BACKEND_TARGETS_AARCH64_INST_ARITHMETIC_ARITHMETIC_H
#define BACKEND_TARGETS_AARCH64_INST_ARITHMETIC_ARITHMETIC_H

#include "backend/targets/utils/inst/targetinst.h"
namespace backend::targetir::aarch64 {
// In aarch64, the arithmetic instructions include
// add, sub, mul, div

#define AARCH64_ARITHMETIC_REG_DEFINE(InstType)                                                        \
  class InstType##Inst : public TargetInst {                                                           \
   public:                                                                                             \
    auto Emit(std::ofstream *out) -> void override;                                                    \
                                                                                                       \
   private:                                                                                            \
    static auto Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> InstType##Inst *; \
    friend class InstFactory;                                                                          \
  };

#define AARCH64_ARITHMETIC_IMM_DEFINE(InstType)                                                           \
  class InstType##ImmInst : public TargetInst {                                                           \
   public:                                                                                                \
    auto Emit(std::ofstream *out) -> void override;                                                       \
                                                                                                          \
   private:                                                                                               \
    static auto Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *imm) -> InstType##ImmInst *; \
    friend class InstFactory;                                                                             \
  };

// Define shift type
enum class ShiftType {
  LSL,       // Logical Shift Left
  LSR,       // Logical Shift Right
  ASR,       // Arithmetic Shift Right
  RESERVED,  // Reserved for future use
};

auto ShiftTypeToString(ShiftType type) -> const char * {
  switch (type) {
    case ShiftType::LSL:
      return "LSL";
    case ShiftType::LSR:
      return "LSR";
    case ShiftType::ASR:
      return "ASR";
    default:
      return "RESERVED";
  }
}

#define AARCH64_ARITHMETIC_SHIFT_DEFINE(InstType)                                                       \
  class InstType##ShiftInst : public TargetInst {                                                       \
   public:                                                                                              \
    auto Emit(std::ofstream *out) -> void override;                                                     \
                                                                                                        \
   private:                                                                                             \
    static auto Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2, TargetOperand *shift, \
                       ShiftType type) -> InstType##ShiftInst *;                                        \
    friend class InstFactory;                                                                           \
    ShiftType type;                                                                                     \
  };

AARCH64_ARITHMETIC_REG_DEFINE(ARMADD)
AARCH64_ARITHMETIC_REG_DEFINE(ARMSUB)
AARCH64_ARITHMETIC_REG_DEFINE(ARMMUL)
AARCH64_ARITHMETIC_REG_DEFINE(ARMDIV)
AARCH64_ARITHMETIC_IMM_DEFINE(ARMADD)
AARCH64_ARITHMETIC_IMM_DEFINE(ARMSUB)
AARCH64_ARITHMETIC_SHIFT_DEFINE(ARMADD)
AARCH64_ARITHMETIC_SHIFT_DEFINE(ARMSUB)

// Floating point arithmetic instructions
AARCH64_ARITHMETIC_REG_DEFINE(ARMFADD)
AARCH64_ARITHMETIC_REG_DEFINE(ARMFSUB)
AARCH64_ARITHMETIC_REG_DEFINE(ARMFMUL)
AARCH64_ARITHMETIC_REG_DEFINE(ARMFDIV)
}  // namespace backend::targetir::aarch64
#endif