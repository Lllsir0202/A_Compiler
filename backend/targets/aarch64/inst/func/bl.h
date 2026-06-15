#ifndef BACKEND_TARGETS_AARCH64_INST_FUNC_BL_H
#define BACKEND_TARGETS_AARCH64_INST_FUNC_BL_H

#include "backend/targets/utils/inst/targetinst.h"
namespace backend::targetir::aarch64 {
// Armv8 bl instruction, branch with link, used for function call
class ARMBLInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  ARMBLInst() = default;
  static auto Create(TargetOperand *label, TargetOperand *rd, const std::vector<TargetOperand *> &args) -> ARMBLInst *;
  friend class InstFactory;
};

}  // namespace backend::targetir::aarch64

#endif