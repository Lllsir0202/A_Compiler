#ifndef BACKEND_TARGETS_AARCH64_MEMORY_STORE_H
#define BACKEND_TARGETS_AARCH64_MEMORY_STORE_H

#include "backend/targets/utils/inst/targetinst.h"
namespace backend::targetir::aarch64 {
class ARMSTRWInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  static auto Create(TargetOperand *rs2, TargetOperand *rs1, TargetOperand *rs2_offset) -> ARMSTRWInst *;
  friend class InstFactory;
};

class ARMSTRWimmInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  static auto Create(TargetOperand *rs2, TargetOperand *rs1, TargetOperand *imm) -> ARMSTRWimmInst *;
  friend class InstFactory;
};

}  // namespace backend::targetir::aarch64

#endif