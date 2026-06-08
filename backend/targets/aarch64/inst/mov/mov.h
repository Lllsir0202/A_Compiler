#ifndef BACKEND_TARGETS_AARCH64_INST_MOV_MOV_H
#define BACKEND_TARGETS_AARCH64_INST_MOV_MOV_H

#include "targets/utils/inst/targetinst.h"
#include "targets/utils/operand/targetoperand.h"

namespace backend::targetir::aarch64 {
class ARMMovInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  static auto Create(TargetOperand *dst, TargetOperand *src) -> ARMMovInst *;
  friend class InstFatory;
};

class ARMMoviInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  static auto Create(TargetOperand *dst, TargetOperand *imm) -> ARMMoviInst *;
  friend class InstFatory;
};

// Floating point move instruction,
// only allow register to register move, not support immediate value move
class ARMFMovInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  static auto Create(TargetOperand *dst, TargetOperand *src) -> ARMFMovInst *;
  friend class InstFatory;
};

// MOVZ, MOVK, MOVN might be used to move immediate value

// movz: set a 16-bit immediate value to a register, and zero the rest of the register
class ARMMovzInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  static auto Create(TargetOperand *dst, TargetOperand *imm, int shift) -> ARMMovzInst *;
  friend class InstFatory;
};

// movk: set a 16-bit immediate value to a register, and keep the rest of the register unchanged
class ARMMovkInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  static auto Create(TargetOperand *dst, TargetOperand *imm, int shift) -> ARMMovkInst *;
  friend class InstFatory;
};

// movn: set a 16-bit immediate value to a register, and invert the rest of the register
class ARMMovnInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  static auto Create(TargetOperand *dst, TargetOperand *imm, int shift) -> ARMMovnInst *;
  friend class InstFatory;
};

}  // namespace backend::targetir::aarch64

#endif