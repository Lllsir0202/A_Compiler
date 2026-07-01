#ifndef BACKEND_TARGETS_AARCH64_INST_CMP_CMP_H
#define BACKEND_TARGETS_AARCH64_INST_CMP_CMP_H

#include "backend/targets/utils/factory/instfactory.h"
#include "backend/targets/utils/inst/targetinst.h"
namespace backend::targetir::aarch64 {
class ARMCMPInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  ARMCMPInst() = default;
  static auto Create(TargetOperand *rs1, TargetOperand *rs2) -> ARMCMPInst *;
  friend class targetir::InstFactory;
};

class ARMFCMPInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  ARMFCMPInst() = default;
  static auto Create(TargetOperand *rs1, TargetOperand *rs2) -> ARMFCMPInst *;
  friend class targetir::InstFactory;
};

// cset rd, cond
// Set rd to 1 if condition is true, otherwise set rd to 0
class ARMCSETInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  ARMCSETInst() = default;
  static auto Create(TargetOperand *rd, TargetOperand *cond) -> ARMCSETInst *;
  friend class targetir::InstFactory;
};

}  // namespace backend::targetir::aarch64

#endif