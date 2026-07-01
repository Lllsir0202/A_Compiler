#ifndef BACKEND_TARGETS_AARCH64_INST_MOV_MOV_H
#define BACKEND_TARGETS_AARCH64_INST_MOV_MOV_H

#include "backend/targets/utils/factory/instfactory.h"
#include "backend/targets/utils/inst/targetinst.h"
#include "backend/targets/utils/operand/targetoperand.h"

namespace backend::targetir::aarch64 {
class ARMMovInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  static auto Create(TargetOperand *dst, TargetOperand *src) -> ARMMovInst *;
  friend class targetir::InstFactory;
};

class ARMMoviInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  static auto Create(TargetOperand *dst, TargetOperand *imm) -> ARMMoviInst *;
  friend class targetir::InstFactory;
};

// Floating point move instruction,
// only allow register to register move, not support immediate value move
class ARMFMovInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  static auto Create(TargetOperand *dst, TargetOperand *src) -> ARMFMovInst *;
  friend class targetir::InstFactory;
};

// MOVZ, MOVK, MOVN might be used to move immediate value

// movz: set a 16-bit immediate value to a register, and zero the rest of the register
class ARMMovzInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  ARMMovzInst() = default;
  static auto Create(TargetOperand *dst, TargetOperand *imm, int shift = 0) -> ARMMovzInst *;
  friend class targetir::InstFactory;
  int shift_ = 0;
};

// movk: set a 16-bit immediate value to a register, and keep the rest of the register unchanged
class ARMMovkInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  ARMMovkInst() = default;
  static auto Create(TargetOperand *dst, TargetOperand *imm, int shift = 0) -> ARMMovkInst *;
  friend class targetir::InstFactory;
  int shift_ = 0;
};

// movn: set a 16-bit immediate value to a register, and invert the rest of the register
class ARMMovnInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  ARMMovnInst() = default;
  static auto Create(TargetOperand *dst, TargetOperand *imm, int shift = 0) -> ARMMovnInst *;
  friend class targetir::InstFactory;
  int shift_ = 0;
};

}  // namespace backend::targetir::aarch64

#endif