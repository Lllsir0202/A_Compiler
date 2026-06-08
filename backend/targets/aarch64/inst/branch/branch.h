#ifndef BACKEND_TARGETS_AARCH64_INST_BRANCH_BRANCH_H
#define BACKEND_TARGETS_AARCH64_INST_BRANCH_BRANCH_H

#include <cstdint>
#include <ratio>
#include "backend/targets/utils/inst/targetinst.h"
namespace backend::targetir::aarch64 {
class ARMBInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  static auto Create(TargetOperand *label) -> ARMBInst *;
  friend class InstFactory;
};

enum class ARMBCondOp : uint8_t {
  EQ,  // Equal
  NE,  // Not Equal

  CS,  // Carry Set / Unsigned >=
  CC,  // Carry Clear / Unsigned <

  MI,  // Minus (Negative)
  PL,  // Plus (Positive or Zero)

  VS,  // Overflow Set
  VC,  // Overflow Clear

  HI,  // Unsigned >
  LS,  // Unsigned <=

  GE,  // Signed >=
  LT,  // Signed <

  GT,  // Signed >
  LE,  // Signed <=

  AL,  // Always

  NV  // Never (reserved)
};

auto CondOpToString(ARMBCondOp op) -> std::string {
  switch (op) {
    case ARMBCondOp::EQ:
      return "eq";
    case ARMBCondOp::NE:
      return "ne";
    case ARMBCondOp::CS:
      return "cs";
    case ARMBCondOp::CC:
      return "cc";
    case ARMBCondOp::MI:
      return "mi";
    case ARMBCondOp::PL:
      return "pl";
    case ARMBCondOp::VS:
      return "vs";
    case ARMBCondOp::VC:
      return "vc";
    case ARMBCondOp::HI:
      return "hi";
    case ARMBCondOp::LS:
      return "ls";
    case ARMBCondOp::GE:
      return "ge";
    case ARMBCondOp::LT:
      return "lt";
    case ARMBCondOp::GT:
      return "gt";
    case ARMBCondOp::LE:
      return "le";
    case ARMBCondOp::AL:
      return "al";
    case ARMBCondOp::NV:
      return "nv";
  }
  return "";
};

/** In armv8, branch is like:
 * @example
 * b.<cond> label
 */
class ARMBCondInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  static auto Create(ARMBCondOp op, TargetOperand *label) -> ARMBCondInst *;
  friend class InstFactory;
  ARMBCondOp op_;
};

}  // namespace backend::targetir::aarch64
#endif